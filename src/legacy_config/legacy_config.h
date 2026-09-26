#pragma once

// The config reader of v0.1.0, the last build that read DyingLightHeadTracking.ini in its
// pre-canonical layout, frozen so a player updating from it is converted exactly as that build
// read the file. Nothing in this folder is ever edited. Three things differ from the reader it
// was taken from: it fills this frozen copy of that build's Config and defaults rather than the
// runtime type, it never writes the file (a missing file reads as the defaults, which is what
// the old reader read back from the file it created there), and it reports a refusal apart from
// an absent file. The core values it took its defaults from are copied into legacy_config.cpp,
// and the sanitizers it called into config_sanitize.h beside it.

#include "cameraunlock/config/legacy_import.h"

#include <cstdint>
#include <string>
#include <vector>

namespace DyingLightHeadTracking::legacy {

enum class ReadStatus {
    Read,
    // No file at the path. Config holds the defaults.
    Absent,
    // The old reader returned false and the mod did not start.
    Refused,
};

struct ReadResult {
    ReadStatus status = ReadStatus::Read;
    // For Refused, what the old reader refused.
    std::string reason;
};

struct Config {
    bool enabled_on_startup = true;
    std::uint16_t udp_port = 4242;
    int data_freshness_ms = 500;

    bool world_space_yaw = true;
    bool show_reticle = true;

    float local_smoothing = static_cast<float>(0.0);
    float remote_smoothing = static_cast<float>(0.15);

    bool position_enabled = true;
    float pos_limit_x = 0.30f;
    float pos_limit_y = 0.20f;
    float pos_limit_y_down = 0.20f;
    float pos_limit_z = 0.40f;
    float pos_limit_z_back = 0.10f;

    bool verbose = false;
    bool ignore_gameplay_gate = false;

    bool collision_enabled = false;
    float collision_radius = 0.15f;
    float collision_release_smoothing = 0.9f;

    int vk_toggle = 0x23;      // End
    int vk_cycle_mode = 0x21;  // Page Up
    int vk_yaw_mode = 0x22;    // Page Down
    int vk_reticle = 0x2D;     // Insert
    bool chord_toggle = true;
    bool chord_cycle_mode = true;
    bool chord_yaw_mode = true;
    bool chord_reticle = true;

    // Call on a default-constructed Config.
    ReadResult Read(const char* iniPath);
};

// Every section and key Read reads, in the order it reads them.
std::vector<cameraunlock::config::LegacyKey> ReadKeys();

}  // namespace DyingLightHeadTracking::legacy
