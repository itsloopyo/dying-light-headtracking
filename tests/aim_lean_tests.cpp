#include "test_support.h"

#include "aim_lean.h"

#include <cstdio>
#include <initializer_list>

using namespace DyingLightHeadTracking;

namespace {

EnginePose Leaning() {
    EnginePose p;
    p.yaw_right = 20.0f;
    p.pitch_up = -10.0f;
    p.roll_ccw = 7.0f;
    p.right = 0.2f;
    p.up = -0.05f;
    p.forward = 0.3f;
    return p;
}

int CheckRotationUntouched(const EnginePose& out) {
    int failures = 0;
    const EnginePose in = Leaning();
    CHECK(out.yaw_right == in.yaw_right);
    CHECK(out.pitch_up == in.pitch_up);
    CHECK(out.roll_ccw == in.roll_ccw);
    return failures;
}

int CheckLeanScaled(const EnginePose& out, double scale) {
    int failures = 0;
    const EnginePose in = Leaning();
    CHECK_NEAR(out.right, in.right * scale, 1e-6);
    CHECK_NEAR(out.up, in.up * scale, 1e-6);
    CHECK_NEAR(out.forward, in.forward * scale, 1e-6);
    return failures;
}

int TestHipPassesThroughInBothModes() {
    int failures = 0;
    for (bool freeLook : {false, true}) {
        AimLean lean;
        for (unsigned long long t = 0; t <= 1000; t += 16) {
            const EnginePose out = lean.Apply(Leaning(), false, freeLook, t);
            failures += CheckRotationUntouched(out);
            failures += CheckLeanScaled(out, 1.0);
        }
    }
    return failures;
}

int TestSightsLockedEasesTheLeanOutAndLeavesRotation() {
    int failures = 0;
    AimLean lean;
    lean.Apply(Leaning(), false, false, 0);
    EnginePose out;
    for (unsigned long long t = 16; t <= 400; t += 16) {
        out = lean.Apply(Leaning(), true, false, t);
        failures += CheckRotationUntouched(out);
    }
    failures += CheckLeanScaled(out, 0.0);
    return failures;
}

int TestTrueFreeLookKeepsTheLeanThroughTheAim() {
    int failures = 0;
    AimLean lean;
    for (unsigned long long t = 0; t <= 400; t += 16) {
        const EnginePose out = lean.Apply(Leaning(), true, true, t);
        failures += CheckRotationUntouched(out);
        failures += CheckLeanScaled(out, 1.0);
    }
    return failures;
}

int TestMidTransitionScalesOnlyTheLean() {
    int failures = 0;
    AimLean lean;
    lean.Apply(Leaning(), false, false, 0);
    lean.Apply(Leaning(), true, false, 1000);
    const EnginePose mid = lean.Apply(Leaning(), true, false, 1000 + cameraunlock::ads::AdsFade::kLowerMs / 2);
    failures += CheckRotationUntouched(mid);
    CHECK(mid.right > 0.0f && mid.right < Leaning().right);
    CHECK_NEAR(mid.up / Leaning().up, mid.right / Leaning().right, 1e-5);
    CHECK_NEAR(mid.forward / Leaning().forward, mid.right / Leaning().right, 1e-5);
    return failures;
}

// Where the lean stood on the frame before a reversal is where it stands on the
// frame of the reversal, whether the aim button or the toggle turned it round.
int TestReversalContinuesFromWhereItWas() {
    int failures = 0;
    for (int byToggle = 0; byToggle < 2; ++byToggle) {
        AimLean lean;
        lean.Apply(Leaning(), false, false, 0);
        lean.Apply(Leaning(), true, false, 100);
        const EnginePose before = lean.Apply(Leaning(), true, false, 160);
        const bool aiming = byToggle == 1;
        const bool freeLook = byToggle == 1;
        const EnginePose after = lean.Apply(Leaning(), aiming, freeLook, 160);
        CHECK_NEAR(after.right, before.right, 1e-6);
        CHECK(before.right > 0.0f && before.right < Leaning().right);
        const EnginePose later = lean.Apply(Leaning(), aiming, freeLook, 176);
        CHECK(later.right > after.right);
        const EnginePose settled = lean.Apply(Leaning(), aiming, freeLook, 1000);
        failures += CheckLeanScaled(settled, 1.0);
    }
    return failures;
}

int TestResetReturnsToTheHip() {
    int failures = 0;
    AimLean lean;
    lean.Apply(Leaning(), false, false, 0);
    lean.Apply(Leaning(), true, false, 16);
    lean.Apply(Leaning(), true, false, 1000);
    lean.Reset();
    failures += CheckLeanScaled(lean.Apply(Leaning(), false, false, 1016), 1.0);
    return failures;
}

int TestAimZoomThreshold() {
    int failures = 0;
    CHECK(!IsAimZoom(1.0f));
    CHECK(!IsAimZoom(0.98f));
    CHECK(!IsAimZoom(1.02f));
    CHECK(IsAimZoom(0.8f));
    return failures;
}

}  // namespace

int RunAimLeanTests() {
    std::printf("aim lean\n");
    int failures = 0;
    failures += TestHipPassesThroughInBothModes();
    failures += TestSightsLockedEasesTheLeanOutAndLeavesRotation();
    failures += TestTrueFreeLookKeepsTheLeanThroughTheAim();
    failures += TestMidTransitionScalesOnlyTheLean();
    failures += TestReversalContinuesFromWhereItWas();
    failures += TestResetReturnsToTheHip();
    failures += TestAimZoomThreshold();
    return failures;
}
