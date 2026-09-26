#pragma once

#include "cameraunlock/config/config_owner.h"
#include "cameraunlock/config/config_table.h"
#include "cameraunlock/config/defaults_file.h"
#include "cameraunlock/config/head_tracking_config.h"
#include "cameraunlock/config/legacy_import.h"

#include <string>

namespace DyingLightHeadTracking {

constexpr const char* kConfigFileName = "CameraUnlock.ini";
// The file every build before the canonical format read, beside kConfigFileName. Imported once
// while kConfigFileName is absent, and never written.
constexpr const char* kLegacyConfigFileName = "DyingLightHeadTracking.ini";
// The game's name as cameraunlock-core's data/games.json spells it.
constexpr const char* kConfigDisplayName = "Dying Light";

// Metres the leaned eye is held off the surface the lean ran into. It has to exceed the camera's
// near clip distance (the first-frame log line reports it) or the surface is culled and the
// player sees through it anyway.
constexpr float kCollisionMarginMetres = 0.15f;

// Core's config with this game's defaults and its two diagnostics switches.
struct Config : cameraunlock::HeadTrackingConfig {
    bool verbose = false;
    bool ignore_gameplay_gate = false;

    Config() { lean_clamp.skin = kCollisionMarginMetres; }
};

// The rows of CameraUnlock.ini. Only the tracking mode pair and WorldSpaceYaw are Writable: the
// mode and yaw hotkeys save the player's choice, and End changes the session only.
cameraunlock::config::ConfigTable<Config> MakeConfigTable();

// DyingLightHeadTracking.ini as v0.1.0 read it (legacy_config/), mapped into Config.
cameraunlock::config::LegacyImport<Config> MakeLegacyImport();

// The owner's options for the files in `folder` (with its trailing separator): the settings in
// CameraUnlock.ini, imported once from DyingLightHeadTracking.ini. The mod passes
// DefaultsFile::PerUser() and a test a scratch file.
cameraunlock::config::ConfigOwnerOptions<Config> MakeConfigOwnerOptions(const std::wstring& folder,
                                                                        cameraunlock::config::DefaultsFile defaults);

}  // namespace DyingLightHeadTracking
