#include "test_support.h"

#include "config_sanitize.h"

#include <cstdio>
#include <limits>

using namespace DyingLightHeadTracking;

namespace {

constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float kInf = std::numeric_limits<float>::infinity();

int SmoothingIsKeptInsideTheUnitRange() {
    int failures = 0;
    CHECK_NEAR(SanitizeSmoothing(0.0f, 0.15f), 0.0, 0.0);
    CHECK_NEAR(SanitizeSmoothing(0.37f, 0.15f), 0.37f, 0.0);
    CHECK_NEAR(SanitizeSmoothing(1.0f, 0.15f), 1.0, 0.0);
    CHECK_NEAR(SanitizeSmoothing(-0.5f, 0.15f), 0.0, 0.0);
    CHECK_NEAR(SanitizeSmoothing(4.0f, 0.15f), 1.0, 0.0);
    CHECK_NEAR(SanitizeSmoothing(kNaN, 0.15f), 0.15f, 0.0);
    // Infinity is not finite, so it takes the fallback rather than clamping to 1.
    CHECK_NEAR(SanitizeSmoothing(kInf, 0.15f), 0.15f, 0.0);
    return failures;
}

int PositionLimitsAreNeverNegative() {
    int failures = 0;
    CHECK_NEAR(SanitizePositionLimit(0.3f, 0.2f), 0.3f, 0.0);
    CHECK_NEAR(SanitizePositionLimit(0.0f, 0.2f), 0.0, 0.0);
    CHECK_NEAR(SanitizePositionLimit(-0.1f, 0.2f), 0.0, 0.0);
    CHECK_NEAR(SanitizePositionLimit(kNaN, 0.2f), 0.2f, 0.0);
    CHECK_NEAR(SanitizePositionLimit(-kInf, 0.2f), 0.2f, 0.0);
    // No upper bound: a large limit is the user's to set.
    CHECK_NEAR(SanitizePositionLimit(7.5f, 0.2f), 7.5f, 0.0);
    return failures;
}

int ClampRangeHoldsItsBounds() {
    int failures = 0;
    CHECK_NEAR(ClampRange(0.01f, 0.02f, 0.5f), 0.02f, 0.0);
    CHECK_NEAR(ClampRange(0.7f, 0.02f, 0.5f), 0.5f, 0.0);
    CHECK_NEAR(ClampRange(0.15f, 0.02f, 0.5f), 0.15f, 0.0);
    CHECK_NEAR(SanitizeFinite(kNaN, 0.15f), 0.15f, 0.0);
    CHECK_NEAR(SanitizeFinite(-3.0f, 0.15f), -3.0f, 0.0);
    return failures;
}

}  // namespace

int RunConfigSanitizeTests() {
    std::printf("config_sanitize\n");
    int failures = 0;
    failures += SmoothingIsKeptInsideTheUnitRange();
    failures += PositionLimitsAreNeverNegative();
    failures += ClampRangeHoldsItsBounds();
    return failures;
}
