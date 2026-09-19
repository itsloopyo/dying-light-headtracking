#include "engine_api.h"

#include "logging.h"

#include "cameraunlock/memory/pe_fingerprint.h"

#include <windows.h>

#include <cstring>

namespace DyingLightHeadTracking::engine {

namespace {

constexpr const char* kEngineDll = "engine_x64_rwdi.dll";

// MSVC x64 member-function shape: `this` in rcx, and for a class larger than
// eight bytes returned by value the hidden buffer pointer in rdx.
using GetFloatFn = float (*)(Camera*);
using GetPtrFn = const float* (*)(Camera*);
using GetVecFn = Vec3* (*)(Camera*, Vec3*);
using PointToScreenFn = Vec2* (*)(Camera*, Vec2*, const Vec3*);
using IsInFrustumFn = bool (*)(Camera*, const Vec3*);
using LevelBoolFn = bool (*)(Level*);
using LevelNameFn = const char* (*)(Level*);
using LevelCameraFn = Camera* (*)(Level*);
using GameLevelFn = Level* (*)(void*);
using GamePeerCountFn = unsigned int (*)(void*);
using GameVoidFn = void (*)(void*);
using GameIntFn = int (*)(void*);

struct Api {
    HMODULE module = nullptr;
    void* fromForwardUpPos = nullptr;
    void* raytrace = nullptr;

    GetFloatFn getFov = nullptr;
    GetFloatFn getAspect = nullptr;
    GetFloatFn getClipNear = nullptr;
    GetPtrFn getProjection = nullptr;
    GetVecFn getPosition = nullptr;
    GetVecFn getLeft = nullptr;
    GetVecFn getUp = nullptr;
    GetVecFn getForward = nullptr;
    PointToScreenFn pointToScreen = nullptr;
    IsInFrustumFn isInFrustum = nullptr;

    GameLevelFn getActiveLevel = nullptr;
    GamePeerCountFn getPeerCount = nullptr;
    GameVoidFn takeScreenshot = nullptr;
    GameIntFn getScreenWidth = nullptr;
    GameIntFn getScreenHeight = nullptr;

    LevelCameraFn getActiveCamera = nullptr;
    LevelBoolFn isLoading = nullptr;
    LevelBoolFn isTimerFrozen = nullptr;
    LevelBoolFn inputsEnabled = nullptr;
    LevelBoolFn anyViewActive = nullptr;
    LevelNameFn levelName = nullptr;
    LevelBoolFn replEnabled = nullptr;

    // Address of the engine global holding the game's engine object, read out of
    // ILevel::IsLoading's own first instruction. See ResolveGameGlobal.
    void** gameObjectSlot = nullptr;

    bool resolved = false;
};

Api& State() {
    static Api api;
    return api;
}

template <typename T>
bool Bind(HMODULE mod, const char* symbol, T& out) {
    out = reinterpret_cast<T>(reinterpret_cast<void*>(GetProcAddress(mod, symbol)));
    if (!out) Log::Line("ERROR: %s does not export %s", kEngineDll, symbol);
    return out != nullptr;
}

// ILevel::IsLoading opens with a RIP-relative load of the pointer variable that
// holds the game's engine object, and IBaseCamera::GetAspect reads +0x42C of that
// same object exactly as IGame::GetCameraAspect reads +0x42C of the IGame's
// engine object. That identity is what makes the slot usable: with it the mod
// builds a two-word IGame stand-in and lets the game's own GetActiveLevel walk
// from there, rather than pinning the CGame -> CLevel chain's struct offsets to
// one build.
//
// The opcode bytes are checked rather than assumed. A rebuild that spells the
// read differently fails here and leaves the mod dormant, which is the point of
// reading it out of code instead of hardcoding an RVA.
void** ResolveGameGlobal(void* isLoadingFn) {
    const auto* code = static_cast<const unsigned char*>(isLoadingFn);
    if (code[0] != 0x48 || code[1] != 0x8B || code[2] != 0x05) {
        Log::Line("ERROR: ILevel::IsLoading does not open with a RIP-relative qword load "
                  "(read %02X %02X %02X); the game-object global cannot be located",
                  code[0], code[1], code[2]);
        return nullptr;
    }
    std::int32_t rel = 0;
    std::memcpy(&rel, code + 3, sizeof(rel));
    return reinterpret_cast<void**>(const_cast<unsigned char*>(code) + 7 + rel);
}

// A stand-in for the IGame interface. Every IGame member this mod calls is
// non-virtual and reads nothing but [this+8], so two words is all the game sees.
struct GameInterfaceShim {
    void* reserved = nullptr;
    void* engineObject = nullptr;
};

bool CurrentGame(GameInterfaceShim& out) {
    Api& api = State();
    if (!api.resolved || !api.gameObjectSlot) return false;
    void* engineObject = *api.gameObjectSlot;
    if (!engineObject) return false;
    out.engineObject = engineObject;
    return true;
}

// Calls an IGame member through the shim, or returns `noGame` while the engine
// has no game object yet.
template <typename R, typename Fn>
R CallOnGame(Fn fn, R noGame) {
    GameInterfaceShim game;
    if (!CurrentGame(game)) return noGame;
    return fn(&game);
}

Vec3 ReadCameraVec(GetVecFn fn, Camera* cam) {
    Vec3 out;
    if (cam) fn(cam, &out);
    return out;
}

}  // namespace

bool Resolve() {
    Api& api = State();
    if (api.resolved) return true;

    api.module = GetModuleHandleA(kEngineDll);
    if (!api.module) {
        Log::Line("ERROR: %s is not loaded", kEngineDll);
        return false;
    }

    bool ok = true;
    ok = Bind(api.module, "?FromForwardUpPos@IBaseCamera@@UEAAXAEBVvec3@@00@Z", api.fromForwardUpPos) && ok;
    ok = Bind(api.module, "?GetFOV@IBaseCamera@@QEAAMXZ", api.getFov) && ok;
    ok = Bind(api.module, "?GetAspect@IBaseCamera@@QEAAMXZ", api.getAspect) && ok;
    ok = Bind(api.module, "?GetClipNear@IBaseCamera@@QEAAMXZ", api.getClipNear) && ok;
    ok = Bind(api.module, "?GetProjectionMatrix@IBaseCamera@@QEAAAEBVmtx44@@XZ", api.getProjection) && ok;
    ok = Bind(api.module, "?GetPosition@IBaseCamera@@QEBA?BVvec3@@XZ", api.getPosition) && ok;
    ok = Bind(api.module, "?GetLeftVector@IBaseCamera@@QEBA?BVvec3@@XZ", api.getLeft) && ok;
    ok = Bind(api.module, "?GetUpVector@IBaseCamera@@QEBA?BVvec3@@XZ", api.getUp) && ok;
    ok = Bind(api.module, "?GetForwardVector@IBaseCamera@@QEBA?BVvec3@@XZ", api.getForward) && ok;

    ok = Bind(api.module, "?PointToScreen@IBaseCamera@@QEAA?BVvec2@@AEBVvec3@@@Z", api.pointToScreen) && ok;
    ok = Bind(api.module, "?IsInFrustum@IBaseCamera@@QEAA_NAEBVvec3@@@Z", api.isInFrustum) && ok;

    ok = Bind(api.module, "?GetActiveLevel@IGame@@QEAAPEAVILevel@@XZ", api.getActiveLevel) && ok;
    ok = Bind(api.module, "?ReplGetConnectedTargetsCount@IGame@@QEBAIXZ", api.getPeerCount) && ok;
    ok = Bind(api.module, "?GetScreenWidth@IGame@@QEAAHXZ", api.getScreenWidth) && ok;
    ok = Bind(api.module, "?GetScreenHeight@IGame@@QEAAHXZ", api.getScreenHeight) && ok;

    ok = Bind(api.module, "?GetActiveCamera@ILevel@@QEBAPEAVIBaseCamera@@XZ", api.getActiveCamera) && ok;
    ok = Bind(api.module, "?IsLoading@ILevel@@QEBA_NXZ", api.isLoading) && ok;
    ok = Bind(api.module, "?IsTimerFrozen@ILevel@@QEBA_NXZ", api.isTimerFrozen) && ok;
    ok = Bind(api.module, "?InputsEnabled@ILevel@@QEBA_NXZ", api.inputsEnabled) && ok;
    ok = Bind(api.module, "?IsAnyViewActive@ILevel@@QEBA_NXZ", api.anyViewActive) && ok;
    ok = Bind(api.module, "?GetLevelName@ILevel@@QEAAPEBDXZ", api.levelName) && ok;
    ok = Bind(api.module, "?ReplIsReplicationEnabled@ILevel@@QEBA_NXZ", api.replEnabled) && ok;

    // Optional: a diagnostics aid, not something the mod needs to run.
    api.takeScreenshot = reinterpret_cast<GameVoidFn>(
        reinterpret_cast<void*>(GetProcAddress(api.module, "?TakeScreenshot@IGame@@QEAAXXZ")));

    // Optional: without it the aim depth and the lean clamp report "not queried"
    // rather than taking the mod down.
    api.raytrace = reinterpret_cast<void*>(GetProcAddress(
        api.module,
        "?Raytrace@IGSObject@@QEAAEPEBVIRayCache@@EPEAUSCollision@@AEBVvec3@@AEAV4@GPEAVIControlObject@@IIH@Z"));
    if (!api.raytrace) {
        Log::Line("WARN: IGSObject::Raytrace is missing; the reticle depth and the lean "
                  "clamp will have no world query");
    }

    if (!ok) return false;

    api.gameObjectSlot = ResolveGameGlobal(reinterpret_cast<void*>(api.isLoading));
    if (!api.gameObjectSlot) return false;

    api.resolved = true;
    return true;
}

void LogResolution() {
    Api& api = State();
    cameraunlock::memory::PeFingerprint fingerprint{};
    if (cameraunlock::memory::ReadPeFingerprint(api.module, fingerprint)) {
        Log::Line("%s: TimeDateStamp %08X SizeOfImage %08X CheckSum %08X, base %p", kEngineDll,
                  fingerprint.TimeDateStamp, fingerprint.SizeOfImage, fingerprint.CheckSum,
                  reinterpret_cast<void*>(api.module));
    } else {
        Log::Line("%s: PE header unreadable, base %p", kEngineDll,
                  reinterpret_cast<void*>(api.module));
    }
    Log::Line("Camera entry point IBaseCamera::FromForwardUpPos @ %p, game-object slot @ %p",
              api.fromForwardUpPos, reinterpret_cast<void*>(api.gameObjectSlot));
}

void* FromForwardUpPosTarget() { return State().fromForwardUpPos; }
void* RaytraceTarget() { return State().raytrace; }

float CameraFov(Camera* cam) { return cam ? State().getFov(cam) : 0.0f; }
float CameraAspect(Camera* cam) { return cam ? State().getAspect(cam) : 0.0f; }
float CameraClipNear(Camera* cam) { return cam ? State().getClipNear(cam) : 0.0f; }
Mtx44 CameraProjection(Camera* cam) { return cam ? State().getProjection(cam) : nullptr; }

Vec3 CameraPosition(Camera* cam) { return ReadCameraVec(State().getPosition, cam); }
Vec3 CameraLeft(Camera* cam) { return ReadCameraVec(State().getLeft, cam); }
Vec3 CameraUp(Camera* cam) { return ReadCameraVec(State().getUp, cam); }
Vec3 CameraForward(Camera* cam) { return ReadCameraVec(State().getForward, cam); }

Vec2 PointToScreen(Camera* cam, const Vec3& worldPoint) {
    Vec2 out;
    if (cam) State().pointToScreen(cam, &out, &worldPoint);
    return out;
}

bool IsInFrustum(Camera* cam, const Vec3& worldPoint) {
    return cam && State().isInFrustum(cam, &worldPoint);
}

Level* ActiveLevel() { return CallOnGame<Level*>(State().getActiveLevel, nullptr); }

Camera* ActiveCamera(Level* level) { return level ? State().getActiveCamera(level) : nullptr; }
bool LevelIsLoading(Level* level) { return level ? State().isLoading(level) : true; }
bool LevelTimerFrozen(Level* level) { return level ? State().isTimerFrozen(level) : true; }
bool LevelInputsEnabled(Level* level) { return level ? State().inputsEnabled(level) : false; }
bool LevelAnyViewActive(Level* level) { return level ? State().anyViewActive(level) : false; }
const char* LevelName(Level* level) { return level ? State().levelName(level) : nullptr; }
bool LevelReplicationEnabled(Level* level) { return level ? State().replEnabled(level) : false; }

void TakeScreenshot() {
    GameInterfaceShim game;
    if (!State().takeScreenshot || !CurrentGame(game)) return;
    State().takeScreenshot(&game);
}

bool HasScreenshot() { return State().takeScreenshot != nullptr; }

int ScreenWidth() { return CallOnGame(State().getScreenWidth, 0); }
int ScreenHeight() { return CallOnGame(State().getScreenHeight, 0); }
unsigned int ConnectedPeerCount() { return CallOnGame(State().getPeerCount, 0u); }

}  // namespace DyingLightHeadTracking::engine
