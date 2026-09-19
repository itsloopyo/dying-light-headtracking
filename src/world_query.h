#pragma once

#include "view_pose.h"

namespace DyingLightHeadTracking {

struct TraceHit {
    bool queried = false;  // false: no usable trace context yet this session
    bool blocked = false;
    float distance = 0.0f;
    Vec3f point;
};

// The engine's own world raycast, replayed with a context captured from the game.
//
// IGSObject::Raytrace is exported, but its first argument is a live game object
// and its mode and filter arguments are unnamed integers: nothing in the binary
// says which combination means "the things a bullet stops on". Rather than guess
// them, the mod detours Raytrace, watches the game make its own calls, and keeps
// the argument set from a call whose ray runs from the player camera along its
// forward axis. That is the shot's own filter by construction, so the reticle
// and the lean clamp ask the same question the game asks.
namespace world_query {

// Detours IGSObject::Raytrace so a context can be captured. Without it the mod
// still runs; the aim depth and the lean clamp report "not queried".
bool Install();
void Remove();

// Drops the captured replay context on a load or level change.
void ForgetContext();

// Tells the capture where the clean player camera is and which way it faces
// this frame, so it can recognise the game's own aim trace. Called from the
// camera hook.
void NoteCamera(const Vec3f& pos, const Vec3f& forward);

// What the game's own aim trace along the clean view found on its most recent
// call: the point it stopped at, or a definite miss within its range. "Not
// queried" when the game has not made that trace in the last quarter second.
// This is the reticle's depth; it adds no engine call of its own.
TraceHit GameAimHit();

// Casts from `start` along the unit vector `direction` for `maxDistance` world
// units, using the context from the game's most recent aim trace. Reports "not
// queried" when the game has stopped making that trace.
TraceHit Cast(const Vec3f& start, const Vec3f& direction, float maxDistance);

}  // namespace world_query
}  // namespace DyingLightHeadTracking
