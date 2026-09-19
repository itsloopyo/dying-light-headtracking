#include <cstdio>

int RunViewPoseTests();
int RunAimProjectionTests();
int RunGateRulesTests();
int RunConfigSanitizeTests();

int main() {
    std::printf("DyingLightHeadTracking tests\n");

    int failures = 0;
    failures += RunViewPoseTests();
    failures += RunAimProjectionTests();
    failures += RunGateRulesTests();
    failures += RunConfigSanitizeTests();

    if (failures == 0) {
        std::printf("All tests passed\n");
        return 0;
    }
    std::printf("%d test(s) FAILED\n", failures);
    return 1;
}
