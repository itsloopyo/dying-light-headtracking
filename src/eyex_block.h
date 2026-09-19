#pragma once

#include <windows.h>

namespace DyingLightHeadTracking::eyex_block {

// Points every Tobii EyeX import of the engine at a stub that reports failure,
// so the game's own eye tracking (Extended View turning the camera towards the
// gaze, aim at gaze, and the rest) never gets a device and never moves the view
// under the head pose. Returns false only when the import table could not be
// written.
bool Install(HMODULE engine);

// Puts the original import addresses back. Required before this DLL unloads
// while the game keeps running, or the engine would call into freed code.
void Remove();

}  // namespace DyingLightHeadTracking::eyex_block
