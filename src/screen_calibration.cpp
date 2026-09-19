#include "screen_calibration.h"

#include "engine_convert.h"
#include "logging.h"

#include <cmath>

namespace DyingLightHeadTracking {

namespace {

// How far in front of the camera the probe points sit, and how far to the side.
// Ten metres puts them well past the near plane in any Dying Light interior; two
// metres of lateral offset is a large, unambiguous number of pixels at any FOV.
constexpr float kProbeAhead = 10.0f;
constexpr float kProbeSide = 2.0f;

// A probe that moves fewer pixels than this is not a measurement.
constexpr float kMinProbePixels = 4.0f;

}  // namespace

FrameTangents ReadFrameTangents(engine::Camera* cam) {
    FrameTangents out;
    const float* proj = engine::CameraProjection(cam);
    if (!proj) return out;

    // The engine multiplies a world point by rows 0, 1 and 3 of its combined
    // matrix and divides, so element 0 is the clip-x scale and element 5 the
    // clip-y scale. Their magnitudes are cot(half FOV) on each axis.
    const float sx = std::fabs(proj[0]);
    const float sy = std::fabs(proj[5]);
    if (!(sx > 1e-4f) || !(sy > 1e-4f) || !std::isfinite(sx) || !std::isfinite(sy)) return out;

    out.tan_half_h = 1.0f / sx;
    out.tan_half_v = 1.0f / sy;
    out.valid = out.tan_half_h < 10.0f && out.tan_half_v < 10.0f;
    return out;
}

bool CalibrateScreenConvention(engine::Camera* cam, ScreenConvention& out) {
    // Read the camera's CURRENT stored basis rather than the vectors being passed
    // into the hook: PointToScreen projects through the combined matrix the engine
    // last built, and these are the vectors that matrix was built from, so the
    // probe and the projection agree exactly.
    const Vec3f pos = ToVec(engine::CameraPosition(cam));
    const Vec3f fwd = ToVec(engine::CameraForward(cam));
    const Vec3f up = ToVec(engine::CameraUp(cam));
    const Vec3f engineLeft = ToVec(engine::CameraLeft(cam));

    if (std::fabs(Length(fwd) - 1.0f) > 0.05f || std::fabs(Length(up) - 1.0f) > 0.05f) {
        return false;  // camera not initialised yet; try again next frame
    }

    // The engine's own first matrix column, for the record. This mod derives it
    // as up x forward everywhere else; if the engine disagrees, everything built
    // on it would be mirrored, so it is checked rather than trusted.
    const Vec3f derivedLeft = Cross(up, fwd);
    const float agreement = Dot(derivedLeft, engineLeft);
    if (agreement < 0.9f) {
        // Retried every camera update, so said once rather than once a frame.
        static bool loggedLayoutMismatch = false;
        if (!loggedLayoutMismatch) {
            loggedLayoutMismatch = true;
            Log::Line("ERROR: up x forward does not reproduce the camera's own first column "
                      "(dot %.3f). The camera matrix is not the layout this mod was built "
                      "against; head tracking stays off.", static_cast<double>(agreement));
        }
        return false;
    }

    // Which way along the forward column the camera actually looks, from the
    // engine's own frustum test. Everything after this probes in front of it.
    const bool alongInFrustum = engine::IsInFrustum(cam, FromVec(Add(pos, Scale(fwd, kProbeAhead))));
    const bool againstInFrustum =
        engine::IsInFrustum(cam, FromVec(Sub(pos, Scale(fwd, kProbeAhead))));
    if (alongInFrustum == againstInFrustum) {
        return false;  // not rendering yet, or something sits across the whole view
    }
    const float forwardSign = alongInFrustum ? 1.0f : -1.0f;

    const Vec3f ahead = Add(pos, Scale(fwd, kProbeAhead * forwardSign));
    const engine::Vec2 s0 = engine::PointToScreen(cam, FromVec(ahead));
    const engine::Vec2 sSide =
        engine::PointToScreen(cam, FromVec(Add(ahead, Scale(derivedLeft, kProbeSide))));
    const engine::Vec2 sUp =
        engine::PointToScreen(cam, FromVec(Add(ahead, Scale(up, kProbeSide))));

    const float dx = sSide.x - s0.x;
    const float dy = sUp.y - s0.y;
    if (!std::isfinite(dx) || !std::isfinite(dy) ||
        std::fabs(dx) < kMinProbePixels || std::fabs(dy) < kMinProbePixels) {
        return false;  // not rendering yet, or the probes landed on top of each other
    }

    // PointToScreen returns pixels from the top-left, so a point higher up the
    // screen has the SMALLER y.
    out.forward_sign = forwardSign;
    out.right_sign = dx > 0.0f ? 1.0f : -1.0f;
    out.up_sign = dy < 0.0f ? 1.0f : -1.0f;

    Log::Line("Screen convention measured: the camera looks %s its forward column, its first "
              "matrix column points screen %s (probe moved %.1f px), its up column points "
              "screen %s (probe moved %.1f px)",
              out.forward_sign > 0.0f ? "ALONG" : "AGAINST",
              out.right_sign > 0.0f ? "RIGHT" : "LEFT", static_cast<double>(dx),
              out.up_sign > 0.0f ? "UP" : "DOWN", static_cast<double>(dy));
    return true;
}

}  // namespace DyingLightHeadTracking
