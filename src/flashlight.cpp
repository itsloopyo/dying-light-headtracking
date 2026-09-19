#include "flashlight.h"

#include "logging.h"
#include "perf_probe.h"

#include <MinHook.h>
#include <windows.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace DyingLightHeadTracking::flashlight {

namespace {

constexpr const char* kEngineDll = "engine_x64_rwdi.dll";
constexpr const char* kSetWorldDir = "?SetWorldDir@IControlObject@@QEAAXAEBVvec3@@@Z";
constexpr const char* kSetWorldPosition = "?SetWorldPosition@IControlObject@@QEAAXAEBVvec3@@@Z";
constexpr const char* kGetWorldPosition = "?GetWorldPosition@IControlObject@@QEBA?AVvec3@@XZ";
constexpr const char* kLightClass = ".?AVLightObject@@";

// The game's torch sits at the eye and points along the aim, a few degrees off
// it (5.6 measured). A light further out, or aimed elsewhere, is some other
// light and is left alone.
constexpr float kMaxDistanceFromEye = 1.5f;
constexpr float kMaxDegreesFromAim = 15.0f;

// Like the camera's own forward column, a light's world dir points AGAINST the
// way it shines: the game hands SetWorldDir the negated aim.
constexpr float kDirSign = -1.0f;

// The beam turns this much further than the head, so a glance sweeps the torch
// ahead of the view instead of keeping it pinned to screen centre. The lean is
// still applied 1:1: the light comes from the eye.
constexpr float kTorchLead = 1.5f;

using SetVecFn = void (*)(void* object, const Vec3f* v);
using GetVecFn = Vec3f* (*)(const void* object, Vec3f* out);

SetVecFn g_setDirOriginal = nullptr;
SetVecFn g_setPosOriginal = nullptr;
GetVecFn g_getPos = nullptr;
void* g_setDirTarget = nullptr;
void* g_setPosTarget = nullptr;

SRWLOCK g_viewLock = SRWLOCK_INIT;
ViewBasis g_clean;
ViewBasis g_rendered;
bool g_haveView = false;

// The LightObject vtable, once one has been seen, and the vtables known not to
// be it, so the class check is a pointer compare on every later call.
std::atomic<const void*> g_lightVtable{nullptr};
constexpr int kMaxOtherVtables = 256;
std::atomic<const void*> g_otherVtables[kMaxOtherVtables];
std::atomic<int> g_otherCount{0};

// SetWorldPosition is only adjusted for the light whose direction was adjusted
// by the call just before it on the same thread, which is the order the game's
// torch update makes them in.
thread_local const void* t_turnedLight = nullptr;

std::atomic<bool> g_loggedEngaged{false};

bool RttiNameIs(const void* object, const char* expected) {
    __try {
        const auto* vtable = *reinterpret_cast<const std::uintptr_t* const*>(object);
        const auto* col = reinterpret_cast<const std::uint32_t*>(vtable[-1]);
        if (col[0] != 1) return false;
        const std::uintptr_t moduleBase = reinterpret_cast<std::uintptr_t>(col) - col[5];
        const char* name = reinterpret_cast<const char*>(moduleBase + col[3] + 16);
        return std::strcmp(name, expected) == 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool IsLightObject(const void* object) {
    const void* vtable = *reinterpret_cast<const void* const*>(object);
    if (vtable == g_lightVtable.load(std::memory_order_relaxed)) return true;
    const int others = g_otherCount.load(std::memory_order_acquire);
    for (int i = 0; i < others; ++i) {
        if (g_otherVtables[i].load(std::memory_order_relaxed) == vtable) return false;
    }
    if (RttiNameIs(object, kLightClass)) {
        g_lightVtable.store(vtable, std::memory_order_relaxed);
        return true;
    }
    // Racing threads may both append the same vtable; a duplicate costs one
    // compare and nothing else.
    const int slot = g_otherCount.load(std::memory_order_relaxed);
    if (slot < kMaxOtherVtables) {
        g_otherVtables[slot].store(vtable, std::memory_order_relaxed);
        g_otherCount.store(slot + 1, std::memory_order_release);
    }
    return false;
}

bool ReadView(ViewBasis& clean, ViewBasis& rendered) {
    AcquireSRWLockShared(&g_viewLock);
    const bool have = g_haveView;
    clean = g_clean;
    rendered = g_rendered;
    ReleaseSRWLockShared(&g_viewLock);
    return have;
}

void SetWorldDirDetour(void* object, const Vec3f* dir) {
    perf_probe::Scope perf(perf_probe::kTorchDir);
    t_turnedLight = nullptr;
    ViewBasis clean, rendered;
    if (object && dir && IsLightObject(object) && ReadView(clean, rendered)) {
        Vec3f pos;
        g_getPos(object, &pos);
        const float len = Length(*dir);
        const float cosToAim = len > 1e-6f ? Dot(*dir, clean.forward) * kDirSign / len : -1.0f;
        if (Length(Sub(pos, clean.pos)) <= kMaxDistanceFromEye &&
            cosToAim >= std::cos(kMaxDegreesFromAim / 57.2957795f)) {
            if (!g_loggedEngaged.exchange(true, std::memory_order_relaxed)) {
                Log::Line("Flashlight: light %p is aimed along the view from the eye; turning it "
                          "%.1fx the head rotation", object, static_cast<double>(kTorchLead));
            }
            const Vec3f turned = TurnWithHeadScaled(*dir, clean, rendered, kTorchLead);
            t_turnedLight = object;
            perf.Pause();
            g_setDirOriginal(object, &turned);
            return;
        }
    }
    perf.Pause();
    g_setDirOriginal(object, dir);
}

void SetWorldPositionDetour(void* object, const Vec3f* pos) {
    perf_probe::Scope perf(perf_probe::kTorchPos);
    ViewBasis clean, rendered;
    if (object && pos && object == t_turnedLight && ReadView(clean, rendered)) {
        t_turnedLight = nullptr;
        const Vec3f moved = Add(*pos, Sub(rendered.pos, clean.pos));
        perf.Pause();
        g_setPosOriginal(object, &moved);
        return;
    }
    perf.Pause();
    g_setPosOriginal(object, pos);
}

bool HookExport(HMODULE mod, const char* symbol, void* detour, void** original, void*& target) {
    target = reinterpret_cast<void*>(GetProcAddress(mod, symbol));
    if (!target) {
        Log::Line("ERROR: %s does not export %s; the flashlight stays on the aim", kEngineDll,
                  symbol);
        return false;
    }
    MH_STATUS status = MH_CreateHook(target, detour, original);
    if (status == MH_OK) status = MH_EnableHook(target);
    if (status != MH_OK) {
        Log::Line("ERROR: hooking %s failed: %s; the flashlight stays on the aim", symbol,
                  MH_StatusToString(status));
        MH_RemoveHook(target);
        target = nullptr;
        return false;
    }
    return true;
}

}  // namespace

bool Install() {
    HMODULE engineMod = GetModuleHandleA(kEngineDll);
    if (!engineMod) return false;
    g_getPos = reinterpret_cast<GetVecFn>(
        reinterpret_cast<void*>(GetProcAddress(engineMod, kGetWorldPosition)));
    if (!g_getPos) {
        Log::Line("ERROR: %s does not export %s; the flashlight stays on the aim", kEngineDll,
                  kGetWorldPosition);
        return false;
    }
    if (!HookExport(engineMod, kSetWorldDir, reinterpret_cast<void*>(&SetWorldDirDetour),
                    reinterpret_cast<void**>(&g_setDirOriginal), g_setDirTarget)) {
        return false;
    }
    if (!HookExport(engineMod, kSetWorldPosition, reinterpret_cast<void*>(&SetWorldPositionDetour),
                    reinterpret_cast<void**>(&g_setPosOriginal), g_setPosTarget)) {
        Remove();
        return false;
    }
    Log::Line("Flashlight: watching IControlObject::SetWorldDir / SetWorldPosition");
    return true;
}

void Remove() {
    for (void** target : {&g_setDirTarget, &g_setPosTarget}) {
        if (!*target) continue;
        MH_DisableHook(*target);
        MH_RemoveHook(*target);
        *target = nullptr;
    }
}

void PublishView(const ViewBasis& clean, const ViewBasis& rendered) {
    AcquireSRWLockExclusive(&g_viewLock);
    g_clean = clean;
    g_rendered = rendered;
    g_haveView = true;
    ReleaseSRWLockExclusive(&g_viewLock);
}

}  // namespace DyingLightHeadTracking::flashlight
