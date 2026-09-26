#include "legacy_config/legacy_config.h"

#include "legacy_config/config_sanitize.h"
#include "logging.h"

#include "cameraunlock/config/ini_reader.h"

#include <string>

namespace DyingLightHeadTracking::legacy {

namespace {

constexpr bool kEnableOnStartup = true;

constexpr int kPort = 4242;
constexpr int kMinPort = 1024;
constexpr int kMaxPort = 65535;
constexpr int kDataFreshnessMs = 500;

constexpr bool kWorldSpaceYaw = true;
constexpr bool kShowReticle = true;

constexpr float kLocalSmoothing = static_cast<float>(0.0);
constexpr float kRemoteSmoothing = static_cast<float>(0.15);

constexpr bool kPositionEnabled = true;
constexpr float kPosLimitX = 0.30f;
constexpr float kPosLimitY = 0.20f;
constexpr float kPosLimitZ = 0.40f;
constexpr float kPosLimitZBack = 0.10f;

constexpr bool kCollisionEnabled = false;
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

template <typename Sanitizer>
float ReadSanitized(const cameraunlock::IniReader& ini, const char* section, const char* key,
                    float fallback, Sanitizer clean) {
    const float raw = ini.ReadFloat(section, key, fallback);
    const float value = clean(raw);
    if (raw != value) {
        Log::Line("WARN: INI %s.%s value %.4f out of range or non-finite; using %.4f", section,
                  key, static_cast<double>(raw), static_cast<double>(value));
    }
    return value;
}

float ReadPositionLimit(const cameraunlock::IniReader& ini, const char* key, float fallback) {
    return ReadSanitized(ini, "Position", key, fallback,
                         [fallback](float v) { return SanitizePositionLimit(v, fallback); });
}

// GetAsyncKeyState is defined for 0x01-0xFE; 0 is the poller's unbound sentinel.
constexpr int kMaxVirtualKey = 0xFE;

int ReadVirtualKey(const cameraunlock::IniReader& ini, const char* key, int fallback) {
    const int vk = ini.ReadHex("Hotkeys", key, fallback);
    if (vk < 0 || vk > kMaxVirtualKey) {
        Log::Line("WARN: INI Hotkeys.%s value 0x%X is not a virtual-key code (0x01-0xFE, or 0 "
                  "to unbind); using 0x%02X", key, vk, fallback);
        return fallback;
    }
    return vk;
}

}  // namespace

ReadResult Config::Read(const char* iniPath) {
    cameraunlock::IniReader ini;
    if (!ini.Open(iniPath)) {
        return {ReadStatus::Absent, {}};
    }

    enabled_on_startup = ini.ReadBool("General", "EnableOnStartup", kEnableOnStartup);
    const int port = ini.ReadInt("General", "Port", kPort);
    if (port < kMinPort || port > kMaxPort) {
        Log::Line("ERROR: INI port %d out of range %d-%d", port, kMinPort, kMaxPort);
        return {ReadStatus::Refused, "[General] Port=" + std::to_string(port) + " is outside " +
                                         std::to_string(kMinPort) + "-" + std::to_string(kMaxPort)};
    }
    udp_port = static_cast<std::uint16_t>(port);

    data_freshness_ms = ini.ReadInt("General", "DataFreshnessMs", kDataFreshnessMs);
    if (data_freshness_ms <= 0) {
        Log::Line("WARN: INI General.DataFreshnessMs %d is not a positive window; using %d",
                  data_freshness_ms, kDataFreshnessMs);
        data_freshness_ms = kDataFreshnessMs;
    }
    world_space_yaw = ini.ReadBool("General", "WorldSpaceYaw", kWorldSpaceYaw);
    show_reticle = ini.ReadBool("General", "ShowReticle", kShowReticle);

    local_smoothing = ReadSanitized(ini, "Smoothing", "LocalSmoothing", kLocalSmoothing,
                                    [](float v) { return SanitizeSmoothing(v, kLocalSmoothing); });
    remote_smoothing = ReadSanitized(ini, "Smoothing", "RemoteSmoothing", kRemoteSmoothing,
                                     [](float v) { return SanitizeSmoothing(v, kRemoteSmoothing); });

    position_enabled = ini.ReadBool("Position", "Enabled", kPositionEnabled);
    pos_limit_x = ReadPositionLimit(ini, "LimitX", kPosLimitX);
    pos_limit_y = ReadPositionLimit(ini, "LimitY", kPosLimitY);
    pos_limit_y_down = ReadPositionLimit(ini, "LimitYDown", pos_limit_y);
    pos_limit_z = ReadPositionLimit(ini, "LimitZ", kPosLimitZ);
    pos_limit_z_back = ReadPositionLimit(ini, "LimitZBack", kPosLimitZBack);

    verbose = ini.ReadBool("Diagnostics", "Verbose", kVerbose);
    ignore_gameplay_gate = ini.ReadBool("Diagnostics", "IgnoreGameplayGate", kIgnoreGameplayGate);

    collision_enabled = ini.ReadBool("Collision", "CollisionEnabled", kCollisionEnabled);
    collision_radius = ReadSanitized(ini, "Collision", "CollisionRadius", kCollisionRadius,
                                     [](float v) {
                                         return ClampRange(SanitizeFinite(v, kCollisionRadius),
                                                           kMinCollisionRadius, kMaxCollisionRadius);
                                     });
    collision_release_smoothing =
        ReadSanitized(ini, "Collision", "CollisionReleaseSmoothing", kCollisionReleaseSmoothing,
                      [](float v) { return SanitizeSmoothing(v, kCollisionReleaseSmoothing); });

    vk_toggle = ReadVirtualKey(ini, "Toggle", kVkToggle);
    vk_cycle_mode = ReadVirtualKey(ini, "CycleMode", kVkCycleMode);
    vk_yaw_mode = ReadVirtualKey(ini, "YawMode", kVkYawMode);
    vk_reticle = ReadVirtualKey(ini, "Reticle", kVkReticle);
    chord_toggle = ini.ReadBool("Hotkeys", "ChordToggle", kChord);
    chord_cycle_mode = ini.ReadBool("Hotkeys", "ChordCycleMode", kChord);
    chord_yaw_mode = ini.ReadBool("Hotkeys", "ChordYawMode", kChord);
    chord_reticle = ini.ReadBool("Hotkeys", "ChordReticle", kChord);
    return {ReadStatus::Read, {}};
}

std::vector<cameraunlock::config::LegacyKey> ReadKeys() {
    return {
        {"General", "EnableOnStartup"},
        {"General", "Port"},
        {"General", "DataFreshnessMs"},
        {"General", "WorldSpaceYaw"},
        {"General", "ShowReticle"},
        {"Smoothing", "LocalSmoothing"},
        {"Smoothing", "RemoteSmoothing"},
        {"Position", "Enabled"},
        {"Position", "LimitX"},
        {"Position", "LimitY"},
        {"Position", "LimitYDown"},
        {"Position", "LimitZ"},
        {"Position", "LimitZBack"},
        {"Diagnostics", "Verbose"},
        {"Diagnostics", "IgnoreGameplayGate"},
        {"Collision", "CollisionEnabled"},
        {"Collision", "CollisionRadius"},
        {"Collision", "CollisionReleaseSmoothing"},
        {"Hotkeys", "Toggle"},
        {"Hotkeys", "CycleMode"},
        {"Hotkeys", "YawMode"},
        {"Hotkeys", "Reticle"},
        {"Hotkeys", "ChordToggle"},
        {"Hotkeys", "ChordCycleMode"},
        {"Hotkeys", "ChordYawMode"},
        {"Hotkeys", "ChordReticle"},
    };
}

}  // namespace DyingLightHeadTracking::legacy
