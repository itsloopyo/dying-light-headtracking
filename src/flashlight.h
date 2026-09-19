#pragma once

#include "view_pose.h"

// Keeps the player's flashlight on the head-tracked view.
//
// The game aims the torch itself every frame: gamedll hands a LightObject the
// player's aim direction through IControlObject::SetWorldDir, then its position
// through SetWorldPosition. Aim is the clean camera, so without this the beam
// stays on the mouse while the view turns with the head. The mod hooks those two
// engine exports and, for the light the game is pointing along the clean view
// from the eye, turns the direction 1.5x as far as the head turned the camera and
// moves the position by the same lean.
namespace DyingLightHeadTracking::flashlight {

bool Install();
void Remove();

// The clean and rendered camera for the frame, from the camera hook. A frame
// with no head pose publishes the clean camera twice, which makes the flashlight
// pass through untouched.
void PublishView(const ViewBasis& clean, const ViewBasis& rendered);

}  // namespace DyingLightHeadTracking::flashlight
