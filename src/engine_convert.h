#pragma once

#include "engine_api.h"
#include "view_pose.h"

namespace DyingLightHeadTracking {

inline Vec3f ToVec(const engine::Vec3& v) { return {v.x, v.y, v.z}; }
inline engine::Vec3 FromVec(const Vec3f& v) { return {v.x, v.y, v.z}; }

}  // namespace DyingLightHeadTracking
