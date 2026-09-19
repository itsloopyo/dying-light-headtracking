#pragma once

#include "engine_api.h"
#include "view_pose.h"

namespace DyingLightHeadTracking {

// The rendered frame's half-angle tangents, read off the engine's own projection
// matrix rather than from its FOV accessor.
//
// GetFOV returns a number in degrees and says nothing about which axis it
// measures, and pairing a vertical FOV with a horizontal aspect is the silent
// failure AGENTS.md describes: the ratio is off by a constant, normal play runs
// at a fixed fraction of the pose, and head tracking just feels weak. The
// projection matrix has no such ambiguity - element 0 scales view x into clip x
// and element 5 scales view y into clip y, so their reciprocals ARE the tangents
// the frame was drawn with, whichever axis the FOV slider meant.
struct FrameTangents {
    bool valid = false;
    float tan_half_h = 0.0f;
    float tan_half_v = 0.0f;
};

FrameTangents ReadFrameTangents(engine::Camera* cam);

// Establishes which way the camera looks along its forward column, from the
// engine's IsInFrustum, and then which way round its other axes run on screen by
// asking PointToScreen where probe points in front of it land. Returns a
// convention with all three signs non-zero on success. Cheap enough to retry per frame until the
// camera is in a state it can answer from; call it until it succeeds, then stop.
bool CalibrateScreenConvention(engine::Camera* cam, ScreenConvention& out);

}  // namespace DyingLightHeadTracking
