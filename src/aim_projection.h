#pragma once

#include "view_pose.h"

namespace DyingLightHeadTracking {

struct ScreenPoint {
    bool valid = false;
    float ndc_x = 0.0f;  // -1 left .. +1 right
    float ndc_y = 0.0f;  // -1 bottom .. +1 top
};

// Anything this close to the eye plane, or behind it, has no stable screen
// position.
constexpr float kMinForwardDistance = 0.05f;

namespace detail {

// Perspective-divides an offset from the rendered eye by its depth along the
// rendered forward axis. Anything not at least minAhead in front has no screen
// position.
inline ScreenPoint ProjectViewOffset(const Vec3f& v, float minAhead, const ViewBasis& rendered,
                                     float tan_half_h, float tan_half_v) {
    ScreenPoint out;
    const float ahead = Dot(v, rendered.forward);
    if (!(ahead > minAhead) || !(tan_half_h > 0.0f) || !(tan_half_v > 0.0f)) {
        return out;
    }
    out.ndc_x = Dot(v, rendered.right) / ahead / tan_half_h;
    out.ndc_y = Dot(v, rendered.up) / ahead / tan_half_v;
    out.valid = std::isfinite(out.ndc_x) && std::isfinite(out.ndc_y);
    return out;
}

}  // namespace detail

// Projects a world point through the view actually rendered: basis to basis, the
// same vectors the camera hook wrote, so the projection cannot disagree with the
// frame. No Euler angles appear here and no rotation composition is re-derived -
// that is the failure AGENTS.md describes, where a generic projection agrees with
// the camera at small single-axis angles and drifts on combined poses.
//
// tan_half_h / tan_half_v are the rendered frame's half-angle tangents, taken
// from the engine's own projection matrix.
inline ScreenPoint ProjectWorldPoint(const Vec3f& point, const ViewBasis& rendered,
                                     float tan_half_h, float tan_half_v) {
    return detail::ProjectViewOffset(Sub(point, rendered.pos), kMinForwardDistance, rendered,
                                     tan_half_h, tan_half_v);
}

// A target with no known range: only the rendered orientation matters. Correct
// for rotation-only tracking, where the render eye and the shot eye are the same
// point, and WRONG the moment a lean separates them - which is why the camera
// hook only falls back to this when the world query says there is definitely
// nothing out there to hit.
inline ScreenPoint ProjectDirection(const Vec3f& direction, const ViewBasis& rendered,
                                    float tan_half_h, float tan_half_v) {
    return detail::ProjectViewOffset(direction, 1e-3f, rendered, tan_half_h, tan_half_v);
}

inline bool OnScreen(const ScreenPoint& p) {
    return p.valid && p.ndc_x >= -1.0f && p.ndc_x <= 1.0f && p.ndc_y >= -1.0f && p.ndc_y <= 1.0f;
}

}  // namespace DyingLightHeadTracking
