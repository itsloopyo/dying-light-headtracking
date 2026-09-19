#include "lean_trace.h"

#include "world_query.h"

namespace DyingLightHeadTracking::lean_trace {

namespace {

// A ray grazing a wall at a shallow angle needs a long overreach to keep the
// standoff along the normal; flooring the cosine bounds that overreach at 4x.
// The engine's raycast reports where the ray stopped but not the surface normal
// this mod can read, so the worst case is assumed rather than measured.
constexpr float kMinApproachCosine = 0.25f;

}  // namespace

cameraunlock::camera::LeanObstruction Query(void* context,
                                            const cameraunlock::math::Vec3& start,
                                            const cameraunlock::math::Vec3& direction,
                                            float maxDistance) {
    cameraunlock::camera::LeanObstruction out;
    const auto* ctx = static_cast<const Context*>(context);

    // maxDistance already carries the clamp's skin once; the extra reach covers
    // the oblique case.
    const float reach = maxDistance + ctx->standoff * (1.0f / kMinApproachCosine - 1.0f);
    const TraceHit hit = world_query::Cast({start.x, start.y, start.z},
                                           {direction.x, direction.y, direction.z}, reach);
    if (!hit.queried) return out;

    out.queried = true;
    if (!hit.blocked) return out;

    // The clamp subtracts the skin from what this reports. Along an oblique ray
    // the eye must stop standoff/cosine short of the surface, so report the hit
    // that much nearer, plus the skin the clamp takes back off.
    const float usable =
        hit.distance - ctx->standoff / kMinApproachCosine + ctx->standoff;
    out.blocked = true;
    out.distance = usable > 0.0f ? usable : 0.0f;
    return out;
}

}  // namespace DyingLightHeadTracking::lean_trace
