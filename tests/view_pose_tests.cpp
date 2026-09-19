#include "test_support.h"

#include "view_pose.h"

#include <cstdio>
#include <initializer_list>

using namespace DyingLightHeadTracking;

namespace {

// A level camera looking along world +Z with world up +Y. The screen directions
// are what the runtime calibration produces; these tests take them as given and
// check that the pose maths moves them the way it says it does.
ViewBasis LevelCamera() {
    ViewBasis b;
    b.pos = {1.0f, 2.0f, 3.0f};
    b.forward = {0.0f, 0.0f, 1.0f};
    b.up = {0.0f, 1.0f, 0.0f};
    b.right = {1.0f, 0.0f, 0.0f};
    return b;
}

ViewBasis PitchedDown(float degrees) {
    ViewBasis b = LevelCamera();
    const float a = degrees * 3.14159265f / 180.0f;
    b.forward = {0.0f, -std::sin(a), std::cos(a)};
    b.up = {0.0f, std::cos(a), std::sin(a)};
    return b;
}

int CheckOrthonormal(const ViewBasis& b) {
    int failures = 0;
    CHECK_NEAR(Length(b.forward), 1.0, 1e-4);
    CHECK_NEAR(Length(b.up), 1.0, 1e-4);
    CHECK_NEAR(Length(b.right), 1.0, 1e-4);
    CHECK_NEAR(Dot(b.forward, b.up), 0.0, 1e-4);
    CHECK_NEAR(Dot(b.forward, b.right), 0.0, 1e-4);
    CHECK_NEAR(Dot(b.up, b.right), 0.0, 1e-4);
    return failures;
}

int TrackerAxesPassThroughAsTheyMeasuredInGame() {
    int failures = 0;
    // The core position processor clamps z to [-limit_z, +limit_z_back], putting
    // the generous forward budget on negative z, and runs x positive to the
    // player's left. A regression here is the "leaning in barely moves, pulling
    // back moves a lot" symptom, which is invisible until someone plays it.
    const EnginePose forwardLean = EnginePoseFromTracker(0, 0, 0, 0.0f, 0.0f, -0.40f);
    CHECK(forwardLean.forward > 0.0f);
    const EnginePose leftLean = EnginePoseFromTracker(0, 0, 0, 0.30f, 0.0f, 0.0f);
    CHECK(leftLean.right < 0.0f);
    const EnginePose up = EnginePoseFromTracker(0, 0, 0, 0.0f, 0.20f, 0.0f);
    CHECK(up.up > 0.0f);
    const EnginePose rot = EnginePoseFromTracker(10.0f, 10.0f, 10.0f, 0, 0, 0);
    CHECK(rot.yaw_right > 0.0f);
    CHECK(rot.pitch_up > 0.0f);
    CHECK(rot.roll_ccw > 0.0f);
    return failures;
}

int CameraLocalYawTurnsTheViewRight() {
    int failures = 0;
    EnginePose pose;
    pose.yaw_right = 20.0f;
    const ViewBasis out = ApplyHeadRotation(LevelCamera(), pose, false);
    failures += CheckOrthonormal(out);
    // Forward has swung toward the clean camera's right.
    CHECK(Dot(out.forward, LevelCamera().right) > 0.3f);
    CHECK_NEAR(out.forward.y, 0.0, 1e-4);
    return failures;
}

int PitchTurnsTheViewUpAndNothingElse() {
    int failures = 0;
    EnginePose pose;
    pose.pitch_up = 25.0f;
    const ViewBasis out = ApplyHeadRotation(LevelCamera(), pose, false);
    failures += CheckOrthonormal(out);
    CHECK(out.forward.y > 0.3f);
    CHECK_NEAR(out.forward.x, 0.0, 1e-4);
    return failures;
}

int RollLeavesTheAimPointAlone() {
    int failures = 0;
    EnginePose pose;
    pose.roll_ccw = 30.0f;
    const ViewBasis out = ApplyHeadRotation(LevelCamera(), pose, false);
    failures += CheckOrthonormal(out);
    // Litmus test 1: with no yaw and no pitch, a pure roll does not move where
    // the camera points, so the reticle cannot leave screen centre.
    CHECK_NEAR(out.forward.x, 0.0, 1e-4);
    CHECK_NEAR(out.forward.y, 0.0, 1e-4);
    CHECK_NEAR(out.forward.z, 1.0, 1e-4);
    // A counter-clockwise roll tips the camera's right axis up.
    CHECK(out.right.y > 0.3f);
    return failures;
}

int WorldSpaceYawKeepsTheHorizonLevel() {
    int failures = 0;
    EnginePose pose;
    pose.yaw_right = 30.0f;
    const ViewBasis out = ApplyHeadRotation(PitchedDown(60.0f), pose, true);
    failures += CheckOrthonormal(out);
    // Litmus test 4: looking well down and yawing, the world spins about the
    // vertical, so the view axis keeps its pitch rather than sweeping an arc.
    CHECK_NEAR(out.forward.y, PitchedDown(60.0f).forward.y, 1e-3);
    // And it went to the right, not the left. The horizontal part of a 60 degree
    // down-pitched forward is cos(60) = 0.5 long, so a 30 degree turn puts
    // 0.5*sin(30) of it on x.
    CHECK_NEAR(out.forward.x, 0.5 * std::sin(30.0 * 3.14159265 / 180.0), 1e-3);
    CHECK_NEAR(out.forward.z, 0.5 * std::cos(30.0 * 3.14159265 / 180.0), 1e-3);
    return failures;
}

int TorchTurnScalesTheHeadRotation() {
    int failures = 0;
    EnginePose pose;
    pose.yaw_right = 20.0f;
    pose.pitch_up = -10.0f;
    pose.roll_ccw = 8.0f;
    const ViewBasis clean = PitchedDown(30.0f);
    const ViewBasis rendered = ApplyHeadRotation(clean, pose, true);

    // At 1.0 it is exactly the head rotation: the clean axes land on the
    // rendered ones.
    const Vec3f same = TurnWithHeadScaled(clean.forward, clean, rendered, 1.0f);
    CHECK_NEAR(same.x, rendered.forward.x, 1e-4);
    CHECK_NEAR(same.y, rendered.forward.y, 1e-4);
    CHECK_NEAR(same.z, rendered.forward.z, 1e-4);

    // A pure yaw of 20 degrees at 1.5 turns the beam 30.
    EnginePose yaw;
    yaw.yaw_right = 20.0f;
    const ViewBasis level = LevelCamera();
    const ViewBasis yawed = ApplyHeadRotation(level, yaw, false);
    const Vec3f lead = TurnWithHeadScaled(level.forward, level, yawed, 1.5f);
    CHECK_NEAR(std::acos(Dot(lead, level.forward)) * 180.0 / 3.14159265, 30.0, 1e-2);
    CHECK(Dot(lead, level.right) > 0.0f);

    // No head rotation leaves the beam alone.
    const Vec3f still = TurnWithHeadScaled(level.forward, level, level, 1.5f);
    CHECK_NEAR(still.z, 1.0, 1e-6);
    return failures;
}

int WorldSpaceYawMatchesCameraLocalAtTheHorizon() {
    int failures = 0;
    EnginePose pose;
    pose.yaw_right = 15.0f;
    const ViewBasis world = ApplyHeadRotation(LevelCamera(), pose, true);
    const ViewBasis local = ApplyHeadRotation(LevelCamera(), pose, false);
    CHECK_NEAR(world.forward.x, local.forward.x, 1e-4);
    CHECK_NEAR(world.forward.y, local.forward.y, 1e-4);
    CHECK_NEAR(world.forward.z, local.forward.z, 1e-4);
    return failures;
}

int LeanIsHorizonLockedNotCameraLocked() {
    int failures = 0;
    EnginePose pose;
    pose.forward = 0.4f;
    // Pitching the camera down must not turn a forward lean into a drop: the
    // vertical limits exist to bound vertical travel, and a camera-local forward
    // would spend that budget without asking.
    const Vec3f lean = LeanWorldOffset(PitchedDown(50.0f), pose);
    CHECK_NEAR(lean.y, 0.0, 1e-4);
    CHECK_NEAR(lean.z, 0.4, 1e-4);

    EnginePose sideways;
    sideways.right = 0.3f;
    const Vec3f side = LeanWorldOffset(LevelCamera(), sideways);
    CHECK_NEAR(side.x, 0.3, 1e-4);
    CHECK_NEAR(side.y, 0.0, 1e-4);

    EnginePose vertical;
    vertical.up = 0.2f;
    const Vec3f rise = LeanWorldOffset(PitchedDown(50.0f), vertical);
    CHECK_NEAR(rise.y, 0.2, 1e-4);
    CHECK_NEAR(rise.z, 0.0, 1e-4);
    return failures;
}

int LeanSurvivesLookingStraightDown() {
    int failures = 0;
    EnginePose pose;
    pose.forward = 0.4f;
    const Vec3f lean = LeanWorldOffset(PitchedDown(90.0f), pose);
    CHECK_NEAR(Length(lean), 0.4, 1e-3);
    CHECK_NEAR(lean.y, 0.0, 1e-3);
    // Straight down, the heading comes from the camera's up axis, which at 90
    // degrees of pitch lies along the old forward.
    CHECK(lean.z > 0.3f);
    return failures;
}

int EngineUpRoundTripsThroughTheConvention() {
    int failures = 0;
    for (float sign : {1.0f, -1.0f}) {
        ScreenConvention c;
        c.forward_sign = sign;
        c.right_sign = 1.0f;
        c.up_sign = sign;
        ViewBasis b = BasisFromEngine({0, 0, 0}, {0, 0, sign}, {0, sign, 0}, c);
        CHECK_NEAR(b.up.y, 1.0, 1e-4);
        CHECK_NEAR(b.forward.z, 1.0, 1e-4);
        CHECK_NEAR(EngineUpFromBasis(b, c).y, static_cast<double>(sign), 1e-4);
        CHECK_NEAR(EngineForwardFromBasis(b, c).z, static_cast<double>(sign), 1e-4);
    }
    return failures;
}

}  // namespace

int RunViewPoseTests() {
    std::printf("view_pose\n");
    int failures = 0;
    failures += TrackerAxesPassThroughAsTheyMeasuredInGame();
    failures += CameraLocalYawTurnsTheViewRight();
    failures += PitchTurnsTheViewUpAndNothingElse();
    failures += RollLeavesTheAimPointAlone();
    failures += WorldSpaceYawKeepsTheHorizonLevel();
    failures += WorldSpaceYawMatchesCameraLocalAtTheHorizon();
    failures += LeanIsHorizonLockedNotCameraLocked();
    failures += LeanSurvivesLookingStraightDown();
    failures += EngineUpRoundTripsThroughTheConvention();
    failures += TorchTurnScalesTheHeadRotation();
    return failures;
}
