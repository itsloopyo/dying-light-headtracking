#pragma once

#include <windows.h>

namespace DyingLightHeadTracking::window_centering {

// Starts a thread that waits for the game window to settle and, if the engine
// left a windowed-mode window at its default placement in the monitor's top-left
// corner, centres it on that monitor's work area. Fullscreen, borderless, and a
// window the player or a WindowOffset setting put somewhere else are left alone.
void Start(HANDLE shutdownEvent);

// Joins that thread. Call after shutdownEvent is set.
void Stop();

}  // namespace DyingLightHeadTracking::window_centering
