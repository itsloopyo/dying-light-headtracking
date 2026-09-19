#pragma once

#include "engine_api.h"
#include "gate_rules.h"

namespace DyingLightHeadTracking {

// What the gate found this frame, kept as a struct so the camera hook can decide
// and the heartbeat can report without asking the engine twice.
struct GateState {
    GateReason reason = GateReason::NoLevel;
    bool in_gameplay = false;
    bool multiplayer = false;
    unsigned int peers = 0;
};

// Re-reads the engine's own state for `level`, the non-null active level. Called
// once per camera update; the engine calls are cheap accessor functions, and
// rate-limiting them would only make the pause gate react a frame or two late.
GateState EvaluateGate(engine::Level* level, bool calibrated);

}  // namespace DyingLightHeadTracking
