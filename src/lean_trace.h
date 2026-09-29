#pragma once

#include "cameraunlock/camera/lean_line_sweep.h"

namespace DyingLightHeadTracking::lean_trace {

// The engine's line cast in core's LineCastFn shape, for LineSweepQuery. The
// context is unused: the cast replays the game's own aim-trace filter, which
// already skips the player's body.
cameraunlock::camera::LineHit Cast(void* context, const cameraunlock::math::Vec3& start,
                                   const cameraunlock::math::Vec3& direction, float length);

}  // namespace DyingLightHeadTracking::lean_trace
