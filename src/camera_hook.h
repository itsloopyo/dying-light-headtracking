#pragma once

#include "config.h"
#include "tracking_runtime.h"

#include <string>

namespace DyingLightHeadTracking {

// Detours IBaseCamera::FromForwardUpPos, the engine entry point the game drives
// the player camera through, and rewrites the forward, up and position it is
// handed. Everything the game decided - where the player is aiming, where a
// round goes, what an enemy can see - was decided before this call, from the
// player's own state, so the rewrite reaches the picture and nothing else.
bool InstallCameraHook(TrackingRuntime& tracking, const Config& cfg);
void RemoveCameraHook();

// Diagnostics for the heartbeat.
unsigned long long CameraUpdateCount();
unsigned long long PosedFrameCount();
std::string DescribeCameraState();

}  // namespace DyingLightHeadTracking
