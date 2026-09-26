#pragma once

#include "config.h"

#include "cameraunlock/camera/lean_clamp.h"

namespace DyingLightHeadTracking::lean_trace {

struct Context {
    // The standoff the clamp holds, in metres. The engine query is a zero-extent
    // ray, so the standoff lives in the clamp's skin and the ray overreaches to
    // cover it.
    float standoff = kCollisionMarginMetres;
};

// LeanQueryFn over the engine's own world raycast. Casts from the clean eye along
// the lean, far enough past it that a surface the view would come to rest against
// is seen before the eye reaches it, and reports the distance the clamp may use.
cameraunlock::camera::LeanObstruction Query(void* context,
                                            const cameraunlock::math::Vec3& start,
                                            const cameraunlock::math::Vec3& direction,
                                            float maxDistance);

}  // namespace DyingLightHeadTracking::lean_trace
