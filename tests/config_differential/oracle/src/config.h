#pragma once

#include <cstdint>

#include "cameraunlock/data/position_settings.h"
#include "cameraunlock/math/smoothing_utils.h"

namespace DyingLightHeadTracking {

// The shipped defaults, in one place. WriteDefaultIni writes these, LoadOrCreate
// falls back to them and Config's members are initialised from them, so a
// default-constructed Config, a freshly written INI and a read of a missing key
// cannot disagree about what the default is.
namespace defaults {
constexpr bool kEnableOnStartup = true;

constexpr int kPort = 4242;
constexpr int kMinPort = 1024;
constexpr int kMaxPort = 65535;
constexpr int kDataFreshnessMs = 500;

constexpr bool kWorldSpaceYaw = true;
constexpr bool kShowReticle = true;

constexpr float kLocalSmoothing = static_cast<float>(cameraunlock::math::kDefaultLocalSmoothing);
constexpr float kRemoteSmoothing = static_cast<float>(cameraunlock::math::kDefaultRemoteSmoothing);

constexpr bool kPositionEnabled = true;
constexpr float kPosLimitX = cameraunlock::PositionSettings{}.limit_x;
constexpr float kPosLimitY = cameraunlock::PositionSettings{}.limit_y;
constexpr float kPosLimitYDown = cameraunlock::PositionSettings{}.limit_y_down;
constexpr float kPosLimitZ = cameraunlock::PositionSettings{}.limit_z;
constexpr float kPosLimitZBack = cameraunlock::PositionSettings{}.limit_z_back;

// Ships off. The lean clamp calls the engine's physics every frame the head is
// off centre, through a trace context captured from the game's own calls, and
// until a log from a real session shows it stopping at real walls at the right
// distance an unverified trace either blocks on nothing or blocks on everything.
constexpr bool kCollisionEnabled = false;
// Metres. Must exceed the camera's near clip distance, which the mod reads from
// the engine and warns about when this is under it.
constexpr float kCollisionRadius = 0.15f;
constexpr float kMinCollisionRadius = 0.02f;
constexpr float kMaxCollisionRadius = 0.5f;
constexpr float kCollisionReleaseSmoothing = 0.9f;

constexpr bool kVerbose = false;
constexpr bool kIgnoreGameplayGate = false;

constexpr int kVkToggle = 0x23;      // End
constexpr int kVkCycleMode = 0x21;   // Page Up
constexpr int kVkYawMode = 0x22;     // Page Down
constexpr int kVkReticle = 0x2D;     // Insert
constexpr bool kChord = true;
}  // namespace defaults

struct Config {
    bool enabled_on_startup = defaults::kEnableOnStartup;
    std::uint16_t udp_port = static_cast<std::uint16_t>(defaults::kPort);
    int data_freshness_ms = defaults::kDataFreshnessMs;

    bool world_space_yaw = defaults::kWorldSpaceYaw;
    bool show_reticle = defaults::kShowReticle;

    float local_smoothing = defaults::kLocalSmoothing;
    float remote_smoothing = defaults::kRemoteSmoothing;

    bool position_enabled = defaults::kPositionEnabled;
    float pos_limit_x = defaults::kPosLimitX;
    float pos_limit_y = defaults::kPosLimitY;
    float pos_limit_y_down = defaults::kPosLimitYDown;
    float pos_limit_z = defaults::kPosLimitZ;
    float pos_limit_z_back = defaults::kPosLimitZBack;

    bool verbose = defaults::kVerbose;
    bool ignore_gameplay_gate = defaults::kIgnoreGameplayGate;

    bool collision_enabled = defaults::kCollisionEnabled;
    float collision_radius = defaults::kCollisionRadius;
    float collision_release_smoothing = defaults::kCollisionReleaseSmoothing;

    int vk_toggle = defaults::kVkToggle;
    int vk_cycle_mode = defaults::kVkCycleMode;
    int vk_yaw_mode = defaults::kVkYawMode;
    int vk_reticle = defaults::kVkReticle;
    bool chord_toggle = defaults::kChord;
    bool chord_cycle_mode = defaults::kChord;
    bool chord_yaw_mode = defaults::kChord;
    bool chord_reticle = defaults::kChord;

    bool LoadOrCreate(const char* iniPath);
};

}  // namespace DyingLightHeadTracking
