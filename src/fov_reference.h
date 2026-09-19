#pragma once

namespace DyingLightHeadTracking::fov_reference {

// The game's own un-zoomed first person field of view: the CameraDefaultFOV
// player variable, in degrees, on the same vertical axis IBaseCamera::GetFOV and
// the projection matrix's element 5 describe. Cutscenes, iron sights and the
// sprint widening all move the live FOV away from it; this does not move.
//
// Unlike everything in engine_api it is not an exported symbol. The variable is
// read through gamedll's own getter, found by the byte signature of the game's
// own read of variable 779 (CameraDefaultFOV). A build that renumbers the
// variable or respells the read fails the scan, and the mod then applies no zoom
// compensation rather than a guessed one.
bool Install();

// False while the game has not built its player variables yet, or when Install
// failed. `degrees` is only written on success.
bool BaseVerticalFovDegrees(float& degrees);


}  // namespace DyingLightHeadTracking::fov_reference
