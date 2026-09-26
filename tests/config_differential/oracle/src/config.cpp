#include "config.h"

#include "config_sanitize.h"
#include "logging.h"

#include "cameraunlock/config/ini_reader.h"

#include <windows.h>

namespace DyingLightHeadTracking {

namespace {

using namespace defaults;

bool FileExists(const char* path) {
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

bool WriteDefaultIni(const char* path) {
    cameraunlock::IniWriter w;
    if (!w.Open(path)) {
        Log::Line("ERROR: could not create %s (error %lu). The mod cannot store its settings; "
                  "check that the game folder is writable.", path, GetLastError());
        return false;
    }
    w.WriteComment(" Dying Light - Head Tracking configuration");
    w.WriteComment(" Lives next to DyingLightGame.exe. Delete it to get these defaults back.");
    w.WriteBlankLine();
    w.WriteSection("General");
    w.WriteBool("EnableOnStartup", kEnableOnStartup);
    w.WriteComment(" UDP port the tracker sends OpenTrack packets to.");
    w.WriteInt("Port", kPort);
    w.WriteComment(" How old the newest packet may be before the view holds its last pose.");
    w.WriteInt("DataFreshnessMs", kDataFreshnessMs);
    w.WriteComment(" Yaw mode: true = horizon-locked (default), false = camera-local.");
    w.WriteBool("WorldSpaceYaw", kWorldSpaceYaw);
    w.WriteComment(" Move the crosshair onto where the shot actually lands. With this off the");
    w.WriteComment(" game's own centre-screen crosshair is left alone and will not match.");
    w.WriteBool("ShowReticle", kShowReticle);
    w.WriteBlankLine();
    w.WriteSection("Smoothing");
    w.WriteComment(" 0.0 (responsive) - 1.0 (heavy). Covers rotation and position.");
    w.WriteComment(" LocalSmoothing applies to a tracker sending to 127.0.0.1 on this PC;");
    w.WriteComment(" RemoteSmoothing applies to any other address, including this PC's own");
    w.WriteComment(" network address and a phone on your network.");
    w.WriteDouble("LocalSmoothing", kLocalSmoothing);
    w.WriteDouble("RemoteSmoothing", kRemoteSmoothing);
    w.WriteBlankLine();
    w.WriteSection("Position");
    w.WriteComment(" 6DOF positional tracking, in metres. The pose is used at 1:1; shape it in");
    w.WriteComment(" your tracker. LimitY bounds travel up and LimitYDown down; LimitZ bounds");
    w.WriteComment(" leaning toward the screen and LimitZBack leaning away from it.");
    w.WriteBool("Enabled", kPositionEnabled);
    w.WriteDouble("LimitX", kPosLimitX);
    w.WriteDouble("LimitY", kPosLimitY);
    w.WriteDouble("LimitYDown", kPosLimitYDown);
    w.WriteDouble("LimitZ", kPosLimitZ);
    w.WriteDouble("LimitZBack", kPosLimitZBack);
    w.WriteBlankLine();
    w.WriteSection("Collision");
    w.WriteComment(" Stop a lean at walls and props instead of pushing the view through them.");
    w.WriteBool("CollisionEnabled", kCollisionEnabled);
    w.WriteComment(" How far the leaned view is held off a surface, in metres.");
    w.WriteDouble("CollisionRadius", kCollisionRadius);
    w.WriteComment(" How gently a lean opens back up once an obstruction clears (0.0 - 1.0).");
    w.WriteDouble("CollisionReleaseSmoothing", kCollisionReleaseSmoothing);
    w.WriteBlankLine();
    w.WriteSection("Diagnostics");
    w.WriteComment(" Writes a detailed trace to the log twice a second, and takes a screenshot");
    w.WriteComment(" through the game's own renderer whenever a file named");
    w.WriteComment(" DyingLightHeadTracking.shot appears next to this one. For troubleshooting;");
    w.WriteComment(" it makes the log grow quickly.");
    w.WriteBool("Verbose", kVerbose);
    w.WriteComment(" Applies the head pose even where the mod would normally stand down - the");
    w.WriteComment(" front end, a pause, a cutscene. It exists to prove the camera hook works");
    w.WriteComment(" at all on a machine where getting into gameplay is awkward, and it is NOT");
    w.WriteComment(" a way to have head tracking in menus: leave it off to play.");
    w.WriteBool("IgnoreGameplayGate", kIgnoreGameplayGate);
    w.WriteBlankLine();
    w.WriteSection("Hotkeys");
    w.WriteComment(" Virtual-key codes. End = toggle, Page Up = cycle tracking mode,");
    w.WriteComment(" Page Down = yaw mode, Insert = crosshair compensation.");
    w.WriteHex("Toggle", kVkToggle);
    w.WriteHex("CycleMode", kVkCycleMode);
    w.WriteHex("YawMode", kVkYawMode);
    w.WriteHex("Reticle", kVkReticle);
    w.WriteComment(" Chord alternatives: Ctrl+Shift+Y / G / H / U for the same four actions.");
    w.WriteBool("ChordToggle", kChord);
    w.WriteBool("ChordCycleMode", kChord);
    w.WriteBool("ChordYawMode", kChord);
    w.WriteBool("ChordReticle", kChord);
    w.Close();
    return true;
}

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

bool Config::LoadOrCreate(const char* iniPath) {
    if (!FileExists(iniPath) && !WriteDefaultIni(iniPath)) {
        return false;
    }

    cameraunlock::IniReader ini;
    if (!ini.Open(iniPath)) {
        Log::Line("ERROR: Failed to open INI: %s", iniPath);
        return false;
    }

    enabled_on_startup = ini.ReadBool("General", "EnableOnStartup", kEnableOnStartup);
    const int port = ini.ReadInt("General", "Port", kPort);
    if (port < kMinPort || port > kMaxPort) {
        Log::Line("ERROR: INI port %d out of range %d-%d", port, kMinPort, kMaxPort);
        return false;
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
    return true;
}

}  // namespace DyingLightHeadTracking
