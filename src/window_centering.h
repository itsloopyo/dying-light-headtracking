#pragma once

#include <windows.h>

namespace DyingLightHeadTracking::window_centering {

// Starts a thread that watches the game window for the whole session and centres
// it whenever the engine has placed a windowed-mode window: when it opens at the
// monitor origin, when it leaves fullscreen, and when the game resizes it in
// place. Fullscreen and borderless are left alone, and so is a window the player
// moved or a WindowOffset setting put somewhere else.
void Start(HANDLE shutdownEvent);

// Joins that thread. Call after shutdownEvent is set.
void Stop();

}  // namespace DyingLightHeadTracking::window_centering
