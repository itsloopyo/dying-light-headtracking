#pragma once

#include <cmath>

namespace DyingLightHeadTracking::legacy {

// Boundary validation for floats read from the user-editable INI. A NaN from a
// malformed value would otherwise reach the smoothing maths and the camera.

inline float SanitizeFinite(float v, float fallback) {
    return std::isfinite(v) ? v : fallback;
}

inline float ClampRange(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// Smoothing must be finite and inside [0,1]; above 1 the smoothing speed goes
// negative. Any value the user sets inside the range reaches the processor
// untouched.
inline float SanitizeSmoothing(float v, float fallback) {
    return ClampRange(SanitizeFinite(v, fallback), 0.0f, 1.0f);
}

// A negative limit hands the position clamp lo > hi and pins the offset.
inline float SanitizePositionLimit(float v, float fallback) {
    const float f = SanitizeFinite(v, fallback);
    return f < 0.0f ? 0.0f : f;
}

}  // namespace DyingLightHeadTracking::legacy
