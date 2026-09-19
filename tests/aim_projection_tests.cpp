#include "test_support.h"

#include "aim_projection.h"
#include "view_pose.h"

#include <cstdio>
#include <initializer_list>

using namespace DyingLightHeadTracking;

namespace {

constexpr float kTanH = 0.8f;   // roughly a 77 degree horizontal FOV
constexpr float kTanV = 0.45f;  // roughly a 48 degree vertical FOV

ViewBasis LevelCamera() {
    ViewBasis b;
    b.pos = {0.0f, 0.0f, 0.0f};
    b.forward = {0.0f, 0.0f, 1.0f};
    b.up = {0.0f, 1.0f, 0.0f};
    b.right = {1.0f, 0.0f, 0.0f};
    return b;
}

// The six release-gate litmus tests, run against the same functions the camera
// hook uses: the projection consumes the basis ApplyHeadRotation produced, so
// nothing here can drift away from what the frame was drawn with.
ViewBasis Rendered(const EnginePose& pose, bool worldYaw = false) {
    ViewBasis b = ApplyHeadRotation(LevelCamera(), pose, worldYaw);
    b.pos = Add(LevelCamera().pos, LeanWorldOffset(LevelCamera(), pose));
    return b;
}

// Where the shot lands, for a clean camera at the origin looking along +Z.
Vec3f ImpactAt(float depth) { return {0.0f, 0.0f, depth}; }

int PureRollKeepsTheReticleCentred() {
    int failures = 0;
    EnginePose pose;
    pose.roll_ccw = 30.0f;
    const ScreenPoint p = ProjectWorldPoint(ImpactAt(10.0f), Rendered(pose), kTanH, kTanV);
    CHECK(p.valid);
    CHECK_NEAR(p.ndc_x, 0.0, 1e-4);
    CHECK_NEAR(p.ndc_y, 0.0, 1e-4);
    return failures;
}

int PurePitchMovesTheReticleVerticallyOnly() {
    int failures = 0;
    EnginePose pose;
    pose.pitch_up = 12.0f;
    const ScreenPoint p = ProjectWorldPoint(ImpactAt(10.0f), Rendered(pose), kTanH, kTanV);
    CHECK(p.valid);
    CHECK_NEAR(p.ndc_x, 0.0, 1e-4);
    // Looking up puts the aim point below centre.
    CHECK(p.ndc_y < -0.1f);
    return failures;
}

int PitchAndRollDoNotWanderHorizontally() {
    int failures = 0;
    // Litmus test 3: the reticle offset must rotate with the frame exactly as the
    // camera does. With roll composed outermost, a pitched-and-rolled camera puts
    // the aim point on a circle about centre, so its distance from centre is the
    // same as with no roll at all.
    EnginePose pitchOnly;
    pitchOnly.pitch_up = 12.0f;
    const ScreenPoint a = ProjectWorldPoint(ImpactAt(10.0f), Rendered(pitchOnly), kTanV, kTanV);

    EnginePose both = pitchOnly;
    both.roll_ccw = 35.0f;
    const ScreenPoint b = ProjectWorldPoint(ImpactAt(10.0f), Rendered(both), kTanV, kTanV);

    CHECK(a.valid && b.valid);
    const double ra = std::sqrt(a.ndc_x * a.ndc_x + a.ndc_y * a.ndc_y);
    const double rb = std::sqrt(b.ndc_x * b.ndc_x + b.ndc_y * b.ndc_y);
    CHECK_NEAR(ra, rb, 1e-3);
    // And it really did rotate rather than staying put.
    CHECK(std::fabs(b.ndc_x - a.ndc_x) > 0.05);
    return failures;
}

int WorldYawLookingDownLeavesTheReticleAlone() {
    int failures = 0;
    // Litmus test 4: straight down, world-space yaw is a pure spin about the view
    // axis. The world turns; the aim point does not leave centre.
    ViewBasis down = LevelCamera();
    down.forward = {0.0f, -1.0f, 0.0f};
    down.up = {0.0f, 0.0f, 1.0f};
    down.right = {1.0f, 0.0f, 0.0f};

    EnginePose pose;
    pose.yaw_right = 40.0f;
    const ViewBasis rendered = ApplyHeadRotation(down, pose, true);
    const ScreenPoint p = ProjectWorldPoint({0.0f, -10.0f, 0.0f}, rendered, kTanH, kTanV);
    CHECK(p.valid);
    CHECK_NEAR(p.ndc_x, 0.0, 1e-3);
    CHECK_NEAR(p.ndc_y, 0.0, 1e-3);
    return failures;
}

// The release gate the task calls out by name: with rotation centred, a lean must
// keep the mark on the impact point at EVERY range, not just one. A fixed or
// stale depth produces an error of lean * (1/d0 - 1/d), which is zero only at
// d == d0 and changes sign either side of it - so testing near and far, and both
// lean directions, is what catches it.
int PureXLeanHoldsTheMarkNearAndFar() {
    int failures = 0;
    for (float trackerX : {0.30f, -0.30f}) {
        const EnginePose pose = EnginePoseFromTracker(0, 0, 0, trackerX, 0.0f, 0.0f);
        const ViewBasis rendered = Rendered(pose);
        for (float depth : {1.0f, 3.0f, 25.0f}) {
            const ScreenPoint p = ProjectWorldPoint(ImpactAt(depth), rendered, kTanH, kTanV);
            CHECK(p.valid);
            // The mark sits where the impact point projects, which is the lean's
            // parallax and nothing else.
            const double expected = (-pose.right / depth) / kTanH;
            CHECK_NEAR(p.ndc_x, expected, 1e-4);
            CHECK_NEAR(p.ndc_y, 0.0, 1e-4);
        }
    }
    return failures;
}

int PureYLeanHoldsTheMarkNearAndFar() {
    int failures = 0;
    for (float trackerY : {0.20f, -0.20f}) {
        const EnginePose pose = EnginePoseFromTracker(0, 0, 0, 0.0f, trackerY, 0.0f);
        const ViewBasis rendered = Rendered(pose);
        for (float depth : {1.0f, 3.0f, 25.0f}) {
            const ScreenPoint p = ProjectWorldPoint(ImpactAt(depth), rendered, kTanH, kTanV);
            CHECK(p.valid);
            const double expected = (-pose.up / depth) / kTanV;
            CHECK_NEAR(p.ndc_y, expected, 1e-4);
            CHECK_NEAR(p.ndc_x, 0.0, 1e-4);
        }
    }
    return failures;
}

int ADirectionProjectionIgnoresTheLean() {
    int failures = 0;
    // The no-hit fallback projects a direction, which has no parallax term. That
    // is correct for a shot that hits nothing and wrong for one that does, which
    // is why the camera hook only reaches it on a definite miss.
    const EnginePose pose = EnginePoseFromTracker(0, 0, 0, 0.30f, 0.0f, 0.0f);
    const ViewBasis rendered = Rendered(pose);
    const ScreenPoint p = ProjectDirection({0.0f, 0.0f, 1.0f}, rendered, kTanH, kTanV);
    CHECK(p.valid);
    CHECK_NEAR(p.ndc_x, 0.0, 1e-4);
    CHECK_NEAR(p.ndc_y, 0.0, 1e-4);
    return failures;
}

int BehindTheCameraIsNotProjected() {
    int failures = 0;
    const ScreenPoint p = ProjectWorldPoint({0.0f, 0.0f, -5.0f}, LevelCamera(), kTanH, kTanV);
    CHECK(!p.valid);
    CHECK(!OnScreen(p));
    return failures;
}

// Iron sights at AimFov 1.6 narrow the frame by that ratio. With the pose scaled
// by the zoom factor, a world point off to the side lands on the same pixel as it
// did with the raw pose and the un-zoomed frame: the head moves the picture by
// the same amount at any zoom. Roll is not scaled, and does not need to be.
int ZoomScaledPoseMovesThePictureAsFarAsUnzoomed() {
    int failures = 0;
    const float factor = 1.0f / 1.6f;
    const float zoomTanH = kTanH * factor;
    const float zoomTanV = kTanV * factor;

    EnginePose pose;
    pose.yaw_right = 14.0f;
    pose.pitch_up = -9.0f;
    pose.roll_ccw = 11.0f;
    pose.right = 0.2f;
    pose.up = -0.1f;
    const EnginePose scaled = ScalePoseForZoom(pose, factor);
    CHECK_NEAR(scaled.roll_ccw, pose.roll_ccw, 1e-6);
    CHECK_NEAR(ScalePoseForZoom(pose, 1.0f).yaw_right, pose.yaw_right, 1e-4);

    // Each rotation axis on its own is exact.
    EnginePose yawOnly;
    yawOnly.yaw_right = pose.yaw_right;
    EnginePose pitchOnly;
    pitchOnly.pitch_up = pose.pitch_up;
    for (const EnginePose& single : {yawOnly, pitchOnly}) {
        const ScreenPoint wide =
            ProjectDirection(LevelCamera().forward, Rendered(single), kTanH, kTanV);
        const ScreenPoint zoomed = ProjectDirection(
            LevelCamera().forward, Rendered(ScalePoseForZoom(single, factor)), zoomTanH, zoomTanV);
        CHECK(wide.valid && zoomed.valid);
        CHECK_NEAR(zoomed.ndc_x, wide.ndc_x, 1e-4);
        CHECK_NEAR(zoomed.ndc_y, wide.ndc_y, 1e-4);
    }

    EnginePose leanOnly;
    leanOnly.right = 0.25f;
    leanOnly.up = 0.15f;
    const Vec3f near{0.0f, 0.0f, 3.0f};
    const ScreenPoint leanWide = ProjectWorldPoint(near, Rendered(leanOnly), kTanH, kTanV);
    const ScreenPoint leanZoomed =
        ProjectWorldPoint(near, Rendered(ScalePoseForZoom(leanOnly, factor)), zoomTanH, zoomTanV);
    CHECK_NEAR(leanZoomed.ndc_x, leanWide.ndc_x, 1e-4);
    CHECK_NEAR(leanZoomed.ndc_y, leanWide.ndc_y, 1e-4);

    // And without the scaling the zoom really does magnify it, or this test
    // proves nothing.
    const ScreenPoint wide = ProjectDirection(LevelCamera().forward, Rendered(yawOnly), kTanH, kTanV);
    const ScreenPoint unscaled =
        ProjectDirection(LevelCamera().forward, Rendered(yawOnly), zoomTanH, zoomTanV);
    CHECK(std::fabs(unscaled.ndc_x) > std::fabs(wide.ndc_x) * 1.5f);
    return failures;
}

}  // namespace

int RunAimProjectionTests() {
    std::printf("aim_projection\n");
    int failures = 0;
    failures += PureRollKeepsTheReticleCentred();
    failures += PurePitchMovesTheReticleVerticallyOnly();
    failures += PitchAndRollDoNotWanderHorizontally();
    failures += WorldYawLookingDownLeavesTheReticleAlone();
    failures += PureXLeanHoldsTheMarkNearAndFar();
    failures += PureYLeanHoldsTheMarkNearAndFar();
    failures += ADirectionProjectionIgnoresTheLean();
    failures += BehindTheCameraIsNotProjected();
    failures += ZoomScaledPoseMovesThePictureAsFarAsUnzoomed();
    return failures;
}
