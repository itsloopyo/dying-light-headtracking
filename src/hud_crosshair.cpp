#include "hud_crosshair.h"

#include "engine_api.h"
#include "logging.h"
#include "perf_probe.h"

#include "cameraunlock/memory/rtti_vtable.h"

#include <windows.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace DyingLightHeadTracking::hud_crosshair {

namespace {

constexpr const char* kEngineDll = "engine_x64_rwdi.dll";
constexpr const char* kGameDll = "gamedll_x64_rwdi.dll";

// The crosshair is gamedll's HudCrosshair, an IUIElementControl subclass. Its
// vtable is found by RTTI name, so a patch that moves the class does not strand
// the mod.
constexpr const char* kCrosshairClass = "HudCrosshair";

// HudCrosshair's vtable runs to about 200 entries; this bounds the walk if the
// end-of-table test below ever fails to fire.
constexpr int kMaxVtableEntries = 512;

// How far a relative position may drift from the value this mod last wrote
// before it counts as the game having moved the element itself.
constexpr float kPositionTolerance = 0.01f;

// The anchor probe nudges the element by this much in its parent's units and
// reads the top-left position back, so the sign of each axis is the engine's
// answer rather than a reading of the anchor flags.
constexpr float kProbeStep = 16.0f;

// MSVC x64 member functions: `this` in rcx; a vec3 returned by value comes back
// through a hidden pointer in rdx.
using GetVecFn = engine::Vec3* (*)(void*, engine::Vec3*);
using SetVecFn = void (*)(void*, const engine::Vec3*);
using GetElementFn = void* (*)(void*);
using ScreenToLocalFn = void (*)(void*, float, float, float*, float*);
using SetVisibleFn = void (*)(void*, bool);
using IsVisibleFn = bool (*)(void*);
using UiCallbackFn = void (*)(void*);

struct Ui {
    GetVecFn getRelativePos = nullptr;
    GetVecFn getRelativePosTopLeft = nullptr;
    GetVecFn getSize = nullptr;
    SetVecFn setRelativePos = nullptr;
    GetElementFn getParent = nullptr;
    // IUIElement::MouseToUILocalSpace: takes a window pixel (y from the top) and
    // returns where it falls in this element's own local space, by unprojecting
    // through the UI camera and the element's global transform.
    ScreenToLocalFn screenToLocal = nullptr;
    SetVisibleFn setVisible = nullptr;
    IsVisibleFn isVisible = nullptr;
};
Ui g_ui;

template <typename T>
bool Bind(HMODULE mod, const char* symbol, T& out) {
    out = reinterpret_cast<T>(reinterpret_cast<void*>(GetProcAddress(mod, symbol)));
    if (!out) Log::Line("ERROR: %s does not export %s", kEngineDll, symbol);
    return out != nullptr;
}

// --- Aim hand-off from the camera hook --------------------------------------

enum AimState : int { kCentred, kHidden, kAim };

std::atomic<int> g_aimState{kCentred};
std::atomic<std::uint64_t> g_aimNdc{0};

std::uint64_t PackNdc(float x, float y) {
    std::uint32_t bx = 0, by = 0;
    std::memcpy(&bx, &x, sizeof(bx));
    std::memcpy(&by, &y, sizeof(by));
    return (static_cast<std::uint64_t>(by) << 32) | bx;
}

void UnpackNdc(std::uint64_t packed, float& x, float& y) {
    const std::uint32_t bx = static_cast<std::uint32_t>(packed);
    const std::uint32_t by = static_cast<std::uint32_t>(packed >> 32);
    std::memcpy(&x, &bx, sizeof(x));
    std::memcpy(&y, &by, sizeof(y));
}

// --- Per-element state -------------------------------------------------------

// The game runs one element's UI callbacks on more than one worker thread, so
// everything below is touched only under g_elementsLock.
struct Element {
    void* ui = nullptr;
    bool failed = false;
    bool probed = false;
    float signX = 0.0f, signY = 0.0f;
    bool haveBase = false;
    engine::Vec3 base;
    float appliedX = 0.0f, appliedY = 0.0f;
    bool hiddenByUs = false;
};

constexpr int kMaxElements = 4;
Element g_elements[kMaxElements];
bool g_loggedFull = false;

Element* Lookup(void* ui) {
    for (Element& e : g_elements) {
        if (e.ui == ui) return &e;
    }
    for (Element& e : g_elements) {
        if (!e.ui) {
            e = Element{};
            e.ui = ui;
            Log::Line("Game crosshair: HudCrosshair element %p found", ui);
            return &e;
        }
    }
    if (!g_loggedFull) {
        g_loggedFull = true;
        Log::Line("ERROR: more than %d HudCrosshair elements are live; element %p is left where "
                  "the game put it", kMaxElements, ui);
    }
    return nullptr;
}

engine::Vec3 RelativePos(void* ui) {
    engine::Vec3 v;
    g_ui.getRelativePos(ui, &v);
    return v;
}

engine::Vec3 TopLeft(void* ui) {
    engine::Vec3 v;
    g_ui.getRelativePosTopLeft(ui, &v);
    return v;
}

float Sign(float v) { return v > 0.0f ? 1.0f : (v < 0.0f ? -1.0f : 0.0f); }

// Settles, per axis, whether a larger relative position moves the element right
// and down in its parent or the other way, which depends on how it is anchored.
bool Probe(Element& e, int width, int height) {
    const engine::Vec3 r0 = RelativePos(e.ui);
    const engine::Vec3 t0 = TopLeft(e.ui);
    const engine::Vec3 nudged{r0.x + kProbeStep, r0.y + kProbeStep, r0.z};
    g_ui.setRelativePos(e.ui, &nudged);
    const engine::Vec3 t1 = TopLeft(e.ui);
    g_ui.setRelativePos(e.ui, &r0);

    e.signX = Sign(t1.x - t0.x);
    e.signY = Sign(t1.y - t0.y);

    engine::Vec3 size;
    g_ui.getSize(e.ui, &size);
    void* parent = g_ui.getParent(e.ui);
    float parentCx = 0.0f, parentCy = 0.0f;
    if (parent) g_ui.screenToLocal(parent, width * 0.5f, height * 0.5f, &parentCx, &parentCy);

    Log::Line("Game crosshair %p: relative pos (%.1f %.1f), top-left (%.1f %.1f), size "
              "(%.1f %.1f), nudge moved top-left by (%.1f %.1f); screen %dx%d centre is (%.1f "
              "%.1f) in its parent %p",
              e.ui, static_cast<double>(r0.x), static_cast<double>(r0.y),
              static_cast<double>(t0.x), static_cast<double>(t0.y),
              static_cast<double>(size.x), static_cast<double>(size.y),
              static_cast<double>(t1.x - t0.x), static_cast<double>(t1.y - t0.y), width, height,
              static_cast<double>(parentCx), static_cast<double>(parentCy), parent);

    if (e.signX == 0.0f || e.signY == 0.0f) {
        Log::Line("ERROR: game crosshair %p did not move when its relative position was "
                  "nudged; it is left where the game put it", e.ui);
        return false;
    }
    if (!parent) {
        Log::Line("ERROR: game crosshair %p has no parent element to measure the screen in; it "
                  "is left where the game put it", e.ui);
        return false;
    }
    e.probed = true;
    return true;
}

void SetVisible(Element& e, bool hide) {
    if (hide && !e.hiddenByUs && g_ui.isVisible(e.ui)) {
        g_ui.setVisible(e.ui, false);
        e.hiddenByUs = true;
    } else if (!hide && e.hiddenByUs) {
        g_ui.setVisible(e.ui, true);
        e.hiddenByUs = false;
    }
}

// --- The per-frame write -----------------------------------------------------

void ApplyLocked(void* ui) {
    Element* e = Lookup(ui);
    if (!e || e->failed) return;

    const engine::Vec3 cur = RelativePos(ui);
    if (!e->haveBase || std::fabs(cur.x - (e->base.x + e->appliedX)) > kPositionTolerance ||
        std::fabs(cur.y - (e->base.y + e->appliedY)) > kPositionTolerance) {
        e->base = cur;
        e->appliedX = e->appliedY = 0.0f;
        e->haveBase = true;
    }

    const int width = engine::ScreenWidth();
    const int height = engine::ScreenHeight();
    if (width <= 0 || height <= 0) return;

    // Probed with the element at the game's own position, so the centre
    // reference is taken from where the game drew it.
    if (!e->probed && e->appliedX == 0.0f && e->appliedY == 0.0f) {
        if (!Probe(*e, width, height)) {
            e->failed = true;
            return;
        }
    }
    if (!e->probed) return;

    const int state = g_aimState.load(std::memory_order_acquire);
    SetVisible(*e, state == kHidden);

    float offX = 0.0f, offY = 0.0f;
    if (state == kAim) {
        float ndcX = 0.0f, ndcY = 0.0f;
        UnpackNdc(g_aimNdc.load(std::memory_order_acquire), ndcX, ndcY);
        const float px = (ndcX + 1.0f) * static_cast<float>(width) * 0.5f;
        const float py = (1.0f - ndcY) * static_cast<float>(height) * 0.5f;

        void* parent = g_ui.getParent(ui);
        if (!parent) return;
        float cx = 0.0f, cy = 0.0f, tx = 0.0f, ty = 0.0f;
        g_ui.screenToLocal(parent, width * 0.5f, height * 0.5f, &cx, &cy);
        g_ui.screenToLocal(parent, px, py, &tx, &ty);
        offX = (tx - cx) * e->signX;
        offY = (ty - cy) * e->signY;
    }

    if (offX != e->appliedX || offY != e->appliedY) {
        const engine::Vec3 pos{e->base.x + offX, e->base.y + offY, e->base.z};
        g_ui.setRelativePos(ui, &pos);
        e->appliedX = offX;
        e->appliedY = offY;
    }
}

SRWLOCK g_elementsLock = SRWLOCK_INIT;

void Apply(void* ui) {
    perf_probe::Scope perf(perf_probe::kCrosshair);
    AcquireSRWLockExclusive(&g_elementsLock);
    ApplyLocked(ui);
    ReleaseSRWLockExclusive(&g_elementsLock);
}

// --- Vtable patching ---------------------------------------------------------

struct Slot {
    const char* engineSymbol;
    UiCallbackFn detour;
    void** entry = nullptr;
    UiCallbackFn original = nullptr;
};

void OnPostUpdate(void* thiz);
void OnPostUpdateHierarchy(void* thiz);

// HudCrosshair's per-frame UI callbacks. It overrides neither, so each slot holds
// a gamedll import thunk to the engine's base version. The slot is identified by
// the import it jumps through, not by the engine address it lands on: both are
// empty functions the engine's linker folded onto one shared `ret`, along with
// unrelated ones like CRTTIObject::SetRTTIObjectName. The write is idempotent,
// so which of the two runs last in a frame does not matter.
Slot g_slots[] = {
    {"?OnPostUpdate@IUIElementTimeLine@@UEAAXXZ", &OnPostUpdate},
    {"?OnPostUpdateHierarchy@IUIElement@@UEAAXXZ", &OnPostUpdateHierarchy},
};

void OnPostUpdate(void* thiz) {
    g_slots[0].original(thiz);
    Apply(thiz);
}

void OnPostUpdateHierarchy(void* thiz) {
    g_slots[1].original(thiz);
    Apply(thiz);
}

bool IsExecutable(const void* p) {
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(p, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT) return false;
    return (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                           PAGE_EXECUTE_WRITECOPY)) != 0;
}

// A gamedll import thunk is `jmp qword ptr [rip+rel32]`; returns the IAT entry
// it jumps through, or null for anything else.
const void* ThunkIatEntry(const void* fn) {
    const auto* code = static_cast<const unsigned char*>(fn);
    if (code[0] != 0xFF || code[1] != 0x25) return nullptr;
    std::int32_t rel = 0;
    std::memcpy(&rel, code + 2, sizeof(rel));
    return code + 6 + rel;
}

// The IAT entry through which `module` imports `symbol` from `dll`, read from
// its own import directory by name.
const void* FindIatEntry(HMODULE module, const char* dll, const char* symbol) {
    auto* base = reinterpret_cast<const unsigned char*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return nullptr;
    for (auto* imp = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress);
         imp->Name; ++imp) {
        if (_stricmp(reinterpret_cast<const char*>(base + imp->Name), dll) != 0) continue;
        if (!imp->OriginalFirstThunk) return nullptr;
        const auto* names = reinterpret_cast<const IMAGE_THUNK_DATA64*>(base + imp->OriginalFirstThunk);
        const auto* iat = reinterpret_cast<const IMAGE_THUNK_DATA64*>(base + imp->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++iat) {
            if (IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal)) continue;
            const auto* byName =
                reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
            if (std::strcmp(reinterpret_cast<const char*>(byName->Name), symbol) == 0) return iat;
        }
    }
    return nullptr;
}

bool WriteSlot(void** entry, void* value) {
    DWORD old = 0;
    if (!VirtualProtect(entry, sizeof(void*), PAGE_READWRITE, &old)) return false;
    *entry = value;
    DWORD ignored = 0;
    VirtualProtect(entry, sizeof(void*), old, &ignored);
    return true;
}

}  // namespace

bool Install() {
    HMODULE engineMod = GetModuleHandleA(kEngineDll);
    HMODULE gameMod = GetModuleHandleA(kGameDll);
    if (!engineMod || !gameMod) {
        Log::Line("ERROR: game crosshair: %s or %s is not loaded", kEngineDll, kGameDll);
        return false;
    }

    bool ok = true;
    ok = Bind(engineMod, "?GetRelativePos@IUIElement@@QEBA?AVvec3@@XZ", g_ui.getRelativePos) && ok;
    ok = Bind(engineMod, "?GetRelativePosTopLeft@IUIElement@@QEBA?AVvec3@@XZ",
              g_ui.getRelativePosTopLeft) && ok;
    ok = Bind(engineMod, "?GetSize@IUIElement@@UEBA?AVvec3@@XZ", g_ui.getSize) && ok;
    ok = Bind(engineMod, "?SetRelativePos@IUIElement@@QEAAXAEBVvec3@@@Z", g_ui.setRelativePos) && ok;
    ok = Bind(engineMod, "?GetParent@IUIElement@@QEAAPEAV1@XZ", g_ui.getParent) && ok;
    ok = Bind(engineMod, "?MouseToUILocalSpace@IUIElement@@QEAAXMMAEAM0@Z", g_ui.screenToLocal) && ok;
    ok = Bind(engineMod, "?SetVisible@IUIElement@@QEAAX_N@Z", g_ui.setVisible) && ok;
    ok = Bind(engineMod, "?IsVisible@IUIElement@@QEBA_NXZ", g_ui.isVisible) && ok;
    if (!ok) return false;

    cameraunlock::memory::VtableInfo info{};
    if (!cameraunlock::memory::FindVtableFromRTTI(gameMod, kCrosshairClass, info, 1)) {
        Log::Line("ERROR: %s has no RTTI vtable for %s; the game's crosshair stays at screen "
                  "centre", kGameDll, kCrosshairClass);
        return false;
    }
    void** vtable = reinterpret_cast<void**>(info.vtable_address);

    for (Slot& slot : g_slots) {
        const void* iatEntry = FindIatEntry(gameMod, kEngineDll, slot.engineSymbol);
        if (!iatEntry) {
            Log::Line("ERROR: %s does not import %s", kGameDll, slot.engineSymbol);
            return false;
        }
        int found = -1;
        for (int i = 0; i < kMaxVtableEntries && IsExecutable(vtable[i]); ++i) {
            if (ThunkIatEntry(vtable[i]) == iatEntry) {
                if (found >= 0) {
                    Log::Line("ERROR: %s's vtable calls %s from more than one slot",
                              kCrosshairClass, slot.engineSymbol);
                    return false;
                }
                found = i;
            }
        }
        if (found < 0) {
            Log::Line("ERROR: %s's vtable has no slot calling %s; the game's crosshair stays at "
                      "screen centre", kCrosshairClass, slot.engineSymbol);
            return false;
        }
        slot.entry = &vtable[found];
    }

    for (Slot& slot : g_slots) {
        slot.original = reinterpret_cast<UiCallbackFn>(*slot.entry);
        if (!WriteSlot(slot.entry, reinterpret_cast<void*>(slot.detour))) {
            Log::Line("ERROR: could not patch %s's vtable (error %lu)", kCrosshairClass,
                      GetLastError());
            Remove();
            return false;
        }
    }
    Log::Line("Game crosshair: %s vtable @ %p patched at slots %d and %d", kCrosshairClass,
              static_cast<void*>(vtable), static_cast<int>(g_slots[0].entry - vtable),
              static_cast<int>(g_slots[1].entry - vtable));
    return true;
}

void Remove() {
    for (Slot& slot : g_slots) {
        if (slot.entry && slot.original) WriteSlot(slot.entry, reinterpret_cast<void*>(slot.original));
        slot.entry = nullptr;
    }
}

void PublishAim(bool onScreen, float ndcX, float ndcY) {
    if (!onScreen) {
        g_aimState.store(kHidden, std::memory_order_release);
        return;
    }
    g_aimNdc.store(PackNdc(ndcX, ndcY), std::memory_order_release);
    g_aimState.store(kAim, std::memory_order_release);
}

void PublishCentred() { g_aimState.store(kCentred, std::memory_order_release); }


}  // namespace DyingLightHeadTracking::hud_crosshair
