#include "test_support.h"

#include "gate_rules.h"

#include <cstdio>
#include <cstring>

using namespace DyingLightHeadTracking;

namespace {

bool TextIs(GateReason reason, const char* expected) {
    return std::strcmp(GateReasonText(reason), expected) == 0;
}

// The heartbeat and the verbose trace print these; a changed string is a changed
// log that triage greps for.
int EveryReasonHasItsLogText() {
    int failures = 0;
    CHECK(TextIs(GateReason::Ready, "gameplay"));
    CHECK(TextIs(GateReason::NoLevel, "no level"));
    CHECK(TextIs(GateReason::Loading, "loading"));
    CHECK(TextIs(GateReason::Warmup, "post-load warmup"));
    CHECK(TextIs(GateReason::TimerFrozen, "paused or in a menu"));
    CHECK(TextIs(GateReason::InputsDisabled, "inputs disabled (cutscene, dialogue or UI)"));
    CHECK(TextIs(GateReason::NoActiveView, "no active view"));
    CHECK(TextIs(GateReason::MainMenu, "front end"));
    CHECK(TextIs(GateReason::Multiplayer, "multiplayer session"));
    CHECK(TextIs(GateReason::NotCalibrated, "screen convention not measured yet"));
    CHECK(TextIs(static_cast<GateReason>(999), "unknown"));
    return failures;
}

int FrontEndIsMatchedByNameWithoutCase() {
    int failures = 0;
    CHECK(IsFrontEndLevel("menu"));
    CHECK(IsFrontEndLevel("levels/MainMenu/level.lvl"));
    CHECK(IsFrontEndLevel("FRONTEND"));
    CHECK(IsFrontEndLevel("data/FrontEnd_dl1"));
    CHECK(!IsFrontEndLevel("levels/slums/level.lvl"));
    CHECK(!IsFrontEndLevel("front end"));
    CHECK(!IsFrontEndLevel("men"));
    CHECK(!IsFrontEndLevel(""));
    CHECK(!IsFrontEndLevel(nullptr));
    // A plain substring test with no word boundary: a playable level whose name
    // merely contains "menu" is treated as the front end too.
    CHECK(IsFrontEndLevel("levels/menuetto"));
    return failures;
}

}  // namespace

int RunGateRulesTests() {
    std::printf("gate_rules\n");
    int failures = 0;
    failures += EveryReasonHasItsLogText();
    failures += FrontEndIsMatchedByNameWithoutCase();
    return failures;
}
