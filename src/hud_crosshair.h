#pragma once

namespace DyingLightHeadTracking::hud_crosshair {

// Takes over the position of the game's own crosshair, the gamedll HudCrosshair
// UI element, so it sits on the shot instead of at screen centre. The element is
// found through its RTTI vtable and its per-frame UI callbacks are patched in
// that vtable only, so no other UI element is touched. False means the class or
// the engine's UI exports were not found; the log says which.
bool Install();
void Remove();

// Called by the camera hook once per posed frame with where the shot lands, in
// NDC (x right, y up, -1..1). `onScreen` false means there is no aim point to
// show this frame, and the crosshair is hidden until there is one again.
void PublishAim(bool onScreen, float ndcX, float ndcY);

// No head pose is being applied: the crosshair goes back to where the game put
// it, and is shown again if this mod hid it.
void PublishCentred();

}  // namespace DyingLightHeadTracking::hud_crosshair
