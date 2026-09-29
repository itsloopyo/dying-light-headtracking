#include "lean_trace.h"

#include "world_query.h"

namespace DyingLightHeadTracking::lean_trace {

cameraunlock::camera::LineHit Cast(void*, const cameraunlock::math::Vec3& start,
                                   const cameraunlock::math::Vec3& direction, float length) {
    cameraunlock::camera::LineHit out;
    const TraceHit hit = world_query::Cast({start.x, start.y, start.z},
                                           {direction.x, direction.y, direction.z}, length);
    out.queried = hit.queried;
    out.hit = hit.blocked;
    out.distance = hit.distance;
    out.normal = {hit.normal.x, hit.normal.y, hit.normal.z};
    return out;
}

}  // namespace DyingLightHeadTracking::lean_trace
