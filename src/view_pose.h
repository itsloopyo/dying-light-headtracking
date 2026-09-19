#pragma once

#include "cameraunlock/camera/zoom_compensation.h"

#include <cmath>

namespace DyingLightHeadTracking {

struct Vec3f {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

inline Vec3f Add(const Vec3f& a, const Vec3f& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3f Sub(const Vec3f& a, const Vec3f& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3f Scale(const Vec3f& a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float Dot(const Vec3f& a, const Vec3f& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3f Cross(const Vec3f& a, const Vec3f& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float Length(const Vec3f& a) { return std::sqrt(Dot(a, a)); }
inline Vec3f Normalize(const Vec3f& a) {
    const float len = Length(a);
    return len > 1e-6f ? Scale(a, 1.0f / len) : a;
}

// World up. The engine stores the camera as a 3x4 row-major matrix whose columns
// are its left, up and forward axes plus the position, and the up column of a
// standing player camera reads (0, 1, 0). camera_hook logs the clean up vector on
// its first camera update, so a build that moved this says so in the log instead
// of leaning the lean sideways.
constexpr Vec3f kWorldUp{0.0f, 1.0f, 0.0f};

// Which way round the engine's axes run on screen.
//
// This is measured, never derived. The engine names the camera matrix's first
// column "left" and its third "forward", but which way the view looks along that
// forward, and which side of the picture each other column comes out on, depend
// on the world's handedness and on the projection, and none of that is visible
// from the camera matrix. screen_calibration answers it from the engine itself:
// IsInFrustum for the view direction, then PointToScreen for probe points placed
// in front of the camera.
//
// The view direction has to be settled first. A probe behind the camera still
// projects, mirrored through the centre on both axes, and reads as a
// self-consistent answer with both screen signs flipped.
struct ScreenConvention {
    // view direction = forward_sign * (the engine's forward column). In Dying
    // Light the camera looks along the NEGATIVE of the forward it is handed.
    float forward_sign = 0.0f;
    // right = right_sign * (up x forward), with (up x forward) being the engine's
    // own first matrix column.
    float right_sign = 0.0f;
    // screen up = up_sign * (the engine's up column).
    float up_sign = 0.0f;
};

// The clean camera for one frame, already in screen terms.
struct ViewBasis {
    Vec3f pos;
    Vec3f forward;
    Vec3f up;     // screen up
    Vec3f right;  // screen right
};

inline ViewBasis BasisFromEngine(const Vec3f& pos, const Vec3f& forward, const Vec3f& engineUp,
                                 const ScreenConvention& c) {
    ViewBasis b;
    b.pos = pos;
    const Vec3f engineForward = Normalize(forward);
    b.forward = Scale(engineForward, c.forward_sign);
    b.up = Scale(Normalize(engineUp), c.up_sign);
    b.right = Scale(Cross(Normalize(engineUp), engineForward), c.right_sign);
    return b;
}

// The engine wants its own forward and up columns back, whichever way round
// those were.
inline Vec3f EngineForwardFromBasis(const ViewBasis& b, const ScreenConvention& c) {
    return Scale(b.forward, c.forward_sign);
}

inline Vec3f EngineUpFromBasis(const ViewBasis& b, const ScreenConvention& c) {
    return Scale(b.up, c.up_sign);
}

// A head pose expressed as what it does to the picture: degrees of rotation and
// metres of translation, each named for the direction it moves the view.
struct EnginePose {
    float yaw_right = 0.0f;
    float pitch_up = 0.0f;
    float roll_ccw = 0.0f;
    float right = 0.0f;
    float up = 0.0f;
    float forward = 0.0f;
};

// The one place the tracker's convention meets the engine's.
//
// Rotation: yaw, pitch and roll pass through as sent. Those signs were set by
// the player in game, and they do not depend on the screen convention: a
// rotation composed on a basis with every axis mirrored is the same rotation.
// Roll is signed in this function rather than inside the rotation helper so
// that the camera and the reticle projection read one sign between them.
//
// Position: the core position processor clamps z to [-limit_z, +limit_z_back]
// with the generous 0.40 m budget on NEGATIVE z, which is the forward lean, and
// its x runs positive to the player's left. So forward = -z and right = -x.
// (Builds that measured the screen convention from behind the camera leaned
// along a mirrored basis and needed x and z un-negated to look right.)
inline EnginePose EnginePoseFromTracker(float yaw, float pitch, float roll,
                                        float x, float y, float z) {
    EnginePose p;
    p.yaw_right = yaw;
    p.pitch_up = pitch;
    p.roll_ccw = roll;
    p.right = -x;
    p.up = y;
    p.forward = -z;
    return p;
}

// Holds the head's screen displacement at what it would be at the game's
// un-zoomed field of view. Iron sights, a bow draw and the sprint widening all
// move the FOV, and without this the same head turn sweeps further across the
// picture the moment the player aims. Roll rotates the picture rather than
// translating it, so it is left alone. factor is 1.0 whenever the game is not
// zoomed; see cameraunlock/camera/zoom_compensation.h.
inline EnginePose ScalePoseForZoom(const EnginePose& pose, float factor) {
    EnginePose p = pose;
    p.yaw_right = cameraunlock::camera::ScaleAngleForZoom(pose.yaw_right, factor);
    p.pitch_up = cameraunlock::camera::ScaleAngleForZoom(pose.pitch_up, factor);
    p.right = pose.right * factor;
    p.up = pose.up * factor;
    p.forward = pose.forward * factor;
    return p;
}

namespace detail {

constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

// Rotates the pair (a, b) within their plane, turning a toward b.
inline void TurnToward(Vec3f& a, Vec3f& b, float radians) {
    const float c = std::cos(radians), s = std::sin(radians);
    const Vec3f na = Add(Scale(a, c), Scale(b, s));
    const Vec3f nb = Sub(Scale(b, c), Scale(a, s));
    a = na;
    b = nb;
}

// Rodrigues rotation of v about the unit axis k.
inline Vec3f RotateAbout(const Vec3f& v, const Vec3f& k, float radians) {
    const float c = std::cos(radians), s = std::sin(radians);
    return Add(Add(Scale(v, c), Scale(Cross(k, v), s)), Scale(k, Dot(k, v) * (1.0f - c)));
}

}  // namespace detail

// Head rotation composed on top of the clean camera: yaw, then pitch, then roll.
//
// Every turn is expressed as one basis vector moving toward another, so nothing
// here depends on the handedness of the world - the calibrated basis already
// says which way is right and which is up. aim_projection.h must not re-derive
// this composition: it projects through the basis this produces, so the mark and
// the frame cannot disagree.
//
// With worldSpaceYaw the yaw turns about world up rather than the camera's own up
// axis, so looking left and right keeps the horizon level however far the game
// camera is pitched. The direction of that turn is taken from the basis too.
inline ViewBasis ApplyHeadRotation(const ViewBasis& clean, const EnginePose& pose,
                                   bool worldSpaceYaw) {
    using detail::kDegToRad;
    ViewBasis b = clean;

    if (worldSpaceYaw) {
        // Rotating about world up by a positive angle turns forward toward
        // cross(kWorldUp, forward); whether that is screen right depends on the
        // world's handedness, so ask the basis rather than assuming.
        const Vec3f turnsToward = Cross(kWorldUp, b.forward);
        const float sense = Dot(turnsToward, b.right) >= 0.0f ? 1.0f : -1.0f;
        const float a = pose.yaw_right * kDegToRad * sense;
        b.forward = detail::RotateAbout(b.forward, kWorldUp, a);
        b.up = detail::RotateAbout(b.up, kWorldUp, a);
        b.right = detail::RotateAbout(b.right, kWorldUp, a);
    } else {
        detail::TurnToward(b.forward, b.right, pose.yaw_right * kDegToRad);
    }

    detail::TurnToward(b.forward, b.up, pose.pitch_up * kDegToRad);
    // A counter-clockwise roll of the camera tips its right axis up.
    detail::TurnToward(b.right, b.up, pose.roll_ccw * kDegToRad);

    // Re-orthonormalise against accumulated float drift before the engine rebuilds
    // its matrix from forward and up.
    b.forward = Normalize(b.forward);
    b.up = Normalize(Sub(b.up, Scale(b.forward, Dot(b.up, b.forward))));
    b.right = Normalize(Sub(Sub(b.right, Scale(b.forward, Dot(b.right, b.forward))),
                            Scale(b.up, Dot(b.right, b.up))));
    return b;
}

// The lean, in world units, along the CLEAN camera's horizon-locked axes: a flat
// right, world up and a flat forward. Pitching the game camera down therefore
// does not turn a forward lean into a drop, and the vertical position limits
// bound the vertical travel the player actually gets.
inline Vec3f LeanWorldOffset(const ViewBasis& clean, const EnginePose& pose) {
    Vec3f flatRight = Sub(clean.right, Scale(kWorldUp, Dot(clean.right, kWorldUp)));
    Vec3f flatForward = Sub(clean.forward, Scale(kWorldUp, Dot(clean.forward, kWorldUp)));
    if (Length(flatForward) < 1e-4f) {
        // Looking straight up or down: the camera's up axis carries the heading.
        flatForward = Sub(clean.up, Scale(kWorldUp, Dot(clean.up, kWorldUp)));
        flatForward = Scale(flatForward, Dot(clean.forward, kWorldUp) > 0.0f ? -1.0f : 1.0f);
    }
    if (Length(flatRight) < 1e-4f) {
        // Camera rolled onto its side: recover right from the flattened heading.
        flatRight = Cross(Normalize(flatForward), kWorldUp);
        if (Dot(flatRight, clean.right) < 0.0f) flatRight = Scale(flatRight, -1.0f);
    }
    flatRight = Normalize(flatRight);
    flatForward = Normalize(flatForward);
    return Add(Add(Scale(flatRight, pose.right), Scale(kWorldUp, pose.up)),
               Scale(flatForward, pose.forward));
}

// Turns a world vector by the head rotation that carries the clean camera onto
// the rendered one, scaled by `factor` about the same axis: 1.0 is the head
// rotation itself, 1.5 turns half as far again.
//
// For any rotation R and orthonormal basis e_i, sum(e_i x R e_i) is
// 2 sin(angle) times the unit axis and sum(e_i . R e_i) is 1 + 2 cos(angle), so
// the axis and angle come straight off the two bases.
inline Vec3f TurnWithHeadScaled(const Vec3f& v, const ViewBasis& clean, const ViewBasis& rendered,
                                float factor) {
    const Vec3f twiceSinAxis = Add(Add(Cross(clean.right, rendered.right),
                                       Cross(clean.up, rendered.up)),
                                   Cross(clean.forward, rendered.forward));
    const float trace = Dot(clean.right, rendered.right) + Dot(clean.up, rendered.up) +
                        Dot(clean.forward, rendered.forward);
    const float sinAngle = 0.5f * Length(twiceSinAxis);
    const float cosAngle = 0.5f * (trace - 1.0f);
    if (sinAngle < 1e-6f) return v;  // no head rotation, or a half turn no neck makes
    const float angle = std::atan2(sinAngle, cosAngle);
    return detail::RotateAbout(v, Scale(twiceSinAxis, 0.5f / sinAngle), angle * factor);
}

}  // namespace DyingLightHeadTracking
