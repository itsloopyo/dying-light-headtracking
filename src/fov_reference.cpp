#include "fov_reference.h"

#include "logging.h"

#include "cameraunlock/memory/pattern_scanner.h"

#include <windows.h>

#include <cstdint>
#include <cstring>

namespace DyingLightHeadTracking::fov_reference {

namespace {

constexpr const char* kGameDll = "gamedll_x64_rwdi.dll";

// The game's own read of CameraDefaultFOV, twice over in gamedll:
//
//   mov  rcx, [rip+globalPlayerVariablesOwner]
//   test al, al
//   jz   +7
//   mov  rcx, [reg+0E58h]        ; the player's own owner, when it has one
//   mov  rcx, [rcx+0D0h]         ; the variable set
//   mov  edx, 30Bh               ; 779 = CameraDefaultFOV
//   call GetPlayerVariableFloat
//
// 779 is the index gamedll's own enum-to-name table maps to "CameraDefaultFOV",
// and the caller multiplies the result by pi/180, so it is degrees.
constexpr const char* kSignature =
    "48 8B 0D ?? ?? ?? ?? 84 C0 74 07 48 8B ?? 58 0E 00 00 48 8B 89 D0 00 00 00 "
    "BA 0B 03 00 00 E8 ?? ?? ?? ??";
constexpr int kGlobalDispOffset = 3;
constexpr int kGlobalInsnEnd = 7;
constexpr int kCallDispOffset = 31;
constexpr int kCallInsnEnd = 35;
constexpr int kCameraDefaultFov = 779;
constexpr std::ptrdiff_t kVariableSetOffset = 0xD0;

// A value outside this is not a field of view, whatever the getter returned.
constexpr float kMinPlausibleDeg = 10.0f;
constexpr float kMaxPlausibleDeg = 150.0f;

using GetVariableFloatFn = float (*)(void* variables, int index);

void* const* g_owner = nullptr;
GetVariableFloatFn g_getFloat = nullptr;
bool g_loggedImplausible = false;

const unsigned char* RipTarget(const unsigned char* insn, int dispOffset, int insnEnd) {
    std::int32_t rel = 0;
    std::memcpy(&rel, insn + dispOffset, sizeof(rel));
    return insn + insnEnd + rel;
}

}  // namespace

bool Install() {
    HMODULE gameMod = GetModuleHandleA(kGameDll);
    if (!gameMod) {
        Log::Line("ERROR: FOV reference: %s is not loaded", kGameDll);
        return false;
    }
    const auto* site =
        static_cast<const unsigned char*>(cameraunlock::memory::ScanPattern(gameMod, kSignature));
    if (!site) {
        Log::Line("WARN: the game's read of CameraDefaultFOV was not found in %s; cutscene and "
                  "aim zooms will magnify head movement on this build", kGameDll);
        return false;
    }
    g_owner = reinterpret_cast<void* const*>(RipTarget(site, kGlobalDispOffset, kGlobalInsnEnd));
    g_getFloat = reinterpret_cast<GetVariableFloatFn>(
        const_cast<unsigned char*>(RipTarget(site, kCallDispOffset, kCallInsnEnd)));
    Log::Line("FOV reference: CameraDefaultFOV read at %p, variable owner slot %p, getter %p",
              static_cast<const void*>(site), static_cast<const void*>(g_owner),
              reinterpret_cast<void*>(g_getFloat));
    return true;
}

bool BaseVerticalFovDegrees(float& degrees) {
    if (!g_owner || !g_getFloat) return false;
    auto* owner = static_cast<unsigned char*>(*g_owner);
    if (!owner) return false;
    void* variables = *reinterpret_cast<void**>(owner + kVariableSetOffset);
    if (!variables) return false;
    const float value = g_getFloat(variables, kCameraDefaultFov);
    if (!(value >= kMinPlausibleDeg && value <= kMaxPlausibleDeg)) {
        if (!g_loggedImplausible) {
            g_loggedImplausible = true;
            Log::Line("WARN: CameraDefaultFOV read back as %.3f, which is not a field of view; no "
                      "zoom compensation (logged once)", static_cast<double>(value));
        }
        return false;
    }
    degrees = value;
    return true;
}

}  // namespace DyingLightHeadTracking::fov_reference
