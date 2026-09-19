#include "game_state.h"

#include "engine_api.h"
#include "logging.h"

#include <windows.h>

#include <string>

namespace DyingLightHeadTracking {

namespace {

// After a level finishes loading the camera spends a moment being placed. Head
// tracking applied during that window rides the placement and reads as a lurch.
constexpr unsigned long long kPostLoadWarmupMs = 1500;

bool g_wasLoading = true;
unsigned long long g_loadEndedMs = 0;
const void* g_lastLevel = nullptr;
std::string g_lastLoggedName;

}  // namespace

GateState EvaluateGate(engine::Level* level, bool calibrated) {
    GateState state;

    const char* rawName = engine::LevelName(level);
    const char* levelName = rawName ? rawName : "";

    const unsigned long long nowMs = GetTickCount64();
    if (level != g_lastLevel) {
        g_lastLevel = level;
        g_loadEndedMs = nowMs;
    }
    // Compared in place: copying the name every frame would allocate for any
    // name past the small-string buffer.
    if (g_lastLoggedName != levelName) {
        g_lastLoggedName = levelName;
        Log::Line("Level: \"%s\"", levelName);
    }

    const bool loading = engine::LevelIsLoading(level);
    if (g_wasLoading && !loading) g_loadEndedMs = nowMs;
    g_wasLoading = loading;

    // Dying Light has four-player co-op and the Be the Zombie invasion, and both
    // run through the engine's replicator. Peers connected is the unambiguous
    // case; replication enabled with no peers is a session that is open to them,
    // which is still not somewhere to be moving the camera around. Both are
    // reported so the log can tell them apart.
    state.peers = engine::ConnectedPeerCount();
    state.multiplayer = state.peers > 0 || engine::LevelReplicationEnabled(level);

    if (loading) {
        state.reason = GateReason::Loading;
    } else if (IsFrontEndLevel(levelName)) {
        state.reason = GateReason::MainMenu;
    } else if (state.multiplayer) {
        state.reason = GateReason::Multiplayer;
    } else if (!engine::LevelAnyViewActive(level)) {
        state.reason = GateReason::NoActiveView;
    } else if (engine::LevelTimerFrozen(level)) {
        state.reason = GateReason::TimerFrozen;
    } else if (!engine::LevelInputsEnabled(level)) {
        state.reason = GateReason::InputsDisabled;
    } else if (nowMs - g_loadEndedMs < kPostLoadWarmupMs) {
        state.reason = GateReason::Warmup;
    } else if (!calibrated) {
        state.reason = GateReason::NotCalibrated;
    } else {
        state.reason = GateReason::Ready;
        state.in_gameplay = true;
    }
    return state;
}

}  // namespace DyingLightHeadTracking
