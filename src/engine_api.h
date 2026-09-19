#pragma once

#include <cstdint>

// The boundary to Dying Light's Chrome Engine.
//
// Everything here is reached through EXPORTED, name-mangled symbols in
// engine_x64_rwdi.dll rather than through addresses derived from a scan. The
// engine is a DLL with 6817 decorated exports and gamedll_x64_rwdi.dll calls the
// ones below across the module boundary, so a game patch moves the code but not
// the names, and GetProcAddress finds them again. That is why this mod carries
// no per-build RVA table: there is nothing in it that a patch could silently
// invalidate. The two things a patch COULD break - a symbol disappearing, and
// the one compiler-shaped read of a global described below - are both checked at
// load time, and a failed check leaves the mod dormant with the game untouched.
namespace DyingLightHeadTracking::engine {

// Opaque interface pointers. Each of these is a two-word interface whose engine
// object hangs off +8; nothing here reads that, the game's own exported
// accessors do.
struct Camera;
struct Level;

// The engine's 4x4 projection matrix, as 16 engine-owned floats.
using Mtx44 = const float*;

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

// Pixels: x from the left edge, y from the TOP edge. The engine derives both
// from clip space as (ndc + 1) * extent * 0.5 with the vertical term negated, so
// a point higher up the screen has a SMALLER y.
struct Vec2 {
    float x = 0.0f, y = 0.0f;
};

// Resolves every symbol this mod needs and locates the engine's game-object
// global. False means something was missing and the caller must stay dormant.
// Safe to call more than once.
bool Resolve();

// One line naming the module, its PE fingerprint and what resolved, for triage.
void LogResolution();

// The hook target: the engine's own "set the camera from a forward, an up and a
// position" entry point, which is what the game drives the player camera with.
void* FromForwardUpPosTarget();

// --- Camera accessors, all via the engine's exported member functions --------
float CameraFov(Camera* cam);        // degrees, as the engine reports it
float CameraAspect(Camera* cam);
float CameraClipNear(Camera* cam);
Mtx44 CameraProjection(Camera* cam); // 16 floats, engine-owned
Vec3  CameraPosition(Camera* cam);
Vec3  CameraLeft(Camera* cam);
Vec3  CameraUp(Camera* cam);
Vec3  CameraForward(Camera* cam);

// The engine's own world-to-screen projection, through the combined matrix it
// last rendered with. Used once at startup to settle which way round the camera
// axes run on screen; see screen_calibration.
Vec2  PointToScreen(Camera* cam, const Vec3& worldPoint);

// Whether the engine's own frustum test puts a world point on screen. The one
// unambiguous answer to which way the camera looks.
bool  IsInFrustum(Camera* cam, const Vec3& worldPoint);

// --- Level / game state -----------------------------------------------------
Level* ActiveLevel();
Camera* ActiveCamera(Level* level);
bool LevelIsLoading(Level* level);
bool LevelTimerFrozen(Level* level);
bool LevelInputsEnabled(Level* level);
bool LevelAnyViewActive(Level* level);
const char* LevelName(Level* level);
bool LevelReplicationEnabled(Level* level);
unsigned int ConnectedPeerCount();

// The back buffer's size, as the game reports it.
int ScreenWidth();
int ScreenHeight();

// Asks the engine to write its own screenshot of the frame it just presented,
// as a timestamped .tga under the game's out\ScreenShots directory.
//
// This exists because a desktop screen grab is not reliable evidence here: it
// captures whatever is topmost, which on a machine running several sessions is
// regularly somebody else's game, and PrintWindow on a fullscreen D3D11 swap
// chain hands back a stale frame that reads as "nothing moved". The engine's
// own capture comes from the back buffer.
void TakeScreenshot();
bool HasScreenshot();

// The raw exported IGSObject::Raytrace overload that takes an IRayCache. The
// plain overload only forwards to it with a null cache, so every game raytrace
// passes through this one. For world_query's captured-context replay.
// Null until Resolve() succeeds.
void* RaytraceTarget();

}  // namespace DyingLightHeadTracking::engine
