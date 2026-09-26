#include "config.h"

#include "legacy_config/legacy_config.h"
#include "path_utils.h"

#include "cameraunlock/config/head_tracking_config_table.h"
#include "cameraunlock/input/key_bindings.h"
#include "cameraunlock/tracking/tracking_mode.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace DyingLightHeadTracking {

namespace {

using cameraunlock::config::DropRule;
using cameraunlock::config::DroppedValue;
using cameraunlock::config::ImportResult;
using cameraunlock::config::LegacyInput;
using cameraunlock::input::KeyBinding;
using cameraunlock::input::KeyModifiers;

// A legacy hotkey code and its Ctrl+Shift chord switch as one key list: the code's binding when
// it is a key code, then the chord.
std::string KeyList(int vk, bool chord, char letter, const char* key, std::vector<DroppedValue>& dropped) {
    cameraunlock::config::LegacyVirtualKeyToBindings(vk, "Hotkeys", key, dropped);
    std::vector<KeyBinding> bindings;
    if (vk >= 0x01 && vk <= 0xFE) bindings.push_back({KeyModifiers::kNone, vk});
    if (chord) bindings.push_back({KeyModifiers::kCtrl | KeyModifiers::kShift, letter});
    return cameraunlock::input::FormatKeyBindings(bindings);
}

std::string HexCode(int vk) {
    char text[8];
    std::snprintf(text, sizeof text, "0x%02X", static_cast<unsigned>(vk));
    return text;
}

ImportResult Import(const LegacyInput& input, Config& out) {
    // v0.1.0 opened the file by the ANSI path it built itself, not the one the owner derives, so
    // the import builds it the same way.
    const std::string ansiPath = LegacyAnsiPath(input.path);
    if (ansiPath.empty()) {
        return ImportResult::Refused(
            "its path has no ANSI form within MAX_PATH, so the version that wrote this file could "
            "not open it and did not start");
    }

    legacy::Config c;
    const legacy::ReadResult read = c.Read(ansiPath.c_str());
    if (read.status == legacy::ReadStatus::Refused) {
        return ImportResult::Refused(read.reason);
    }

    std::vector<DroppedValue> dropped;

    out.enable_on_startup = c.enabled_on_startup;
    out.udp_port = c.udp_port;
    out.data_freshness_ms = c.data_freshness_ms;
    out.world_space_yaw = c.world_space_yaw;

    // [Position] Enabled chose only the startup mode: the cycle key reached every mode either
    // way.
    const cameraunlock::TrackingModeChannels mode = cameraunlock::EncodeTrackingMode(
        c.position_enabled ? cameraunlock::TrackingMode::RotationAndPosition
                           : cameraunlock::TrackingMode::RotationOnly);
    out.rotation_enabled = mode.rotation_enabled;
    out.position_enabled = mode.position_enabled;

    out.local_smoothing = c.local_smoothing;
    out.position.local_smoothing = c.local_smoothing;
    out.remote_smoothing = c.remote_smoothing;
    out.position.remote_smoothing = c.remote_smoothing;

    out.position.limit_x = c.pos_limit_x;
    out.position.limit_y = c.pos_limit_y;
    out.position.limit_y_down = c.pos_limit_y_down;
    out.position.limit_z = c.pos_limit_z;
    out.position.limit_z_back = c.pos_limit_z_back;

    // The lean clamp shipped switched off pending verification, so it takes the table's default
    // (approved change follows_default).
    out.collision_enabled = MakeConfigTable().defaults().collision_enabled;
    if (c.collision_enabled != out.collision_enabled) {
        dropped.push_back({DropRule::FollowsDefault, "Collision", "CollisionEnabled", c.collision_enabled ? "1" : "0"});
    }
    out.lean_clamp.skin = c.collision_radius;
    out.lean_clamp.release_smoothing = c.collision_release_smoothing;

    out.verbose = c.verbose;
    out.ignore_gameplay_gate = c.ignore_gameplay_gate;

    // The game's crosshair now always follows the aim, so the switch that left it at the centre
    // and the key that toggled it are gone (approved change reticle). v0.1.0 registered the key
    // for any nonzero code the reader passed, and the chord whenever its switch was on.
    if (!c.show_reticle) dropped.push_back({DropRule::Reticle, "General", "ShowReticle", "0"});
    if (c.vk_reticle != 0) dropped.push_back({DropRule::Reticle, "Hotkeys", "Reticle", HexCode(c.vk_reticle)});
    if (c.chord_reticle) dropped.push_back({DropRule::Reticle, "Hotkeys", "ChordReticle", "1"});

    out.toggle_key_name = KeyList(c.vk_toggle, c.chord_toggle, 'Y', "Toggle", dropped);
    out.cycle_tracking_mode_key_name = KeyList(c.vk_cycle_mode, c.chord_cycle_mode, 'G', "CycleMode", dropped);
    out.yaw_mode_key_name = KeyList(c.vk_yaw_mode, c.chord_yaw_mode, 'H', "YawMode", dropped);

    return read.status == legacy::ReadStatus::Absent ? ImportResult::Absent(std::move(dropped))
                                                     : ImportResult::Imported(std::move(dropped));
}

}  // namespace

cameraunlock::config::ConfigTable<Config> MakeConfigTable() {
    using cameraunlock::config::schema::Concept;
    cameraunlock::config::ConfigTable<Config> table = cameraunlock::config::HeadTrackingConfigTable<Config>(
        {Concept::UdpPort, Concept::EnableOnStartup, Concept::WorldSpaceYaw, Concept::RotationEnabled,
         Concept::DataFreshnessMs, Concept::LocalSmoothing, Concept::RemoteSmoothing, Concept::PositionEnabled,
         Concept::PositionLimitX, Concept::PositionLimitY, Concept::PositionLimitYDown, Concept::PositionLimitZ,
         Concept::PositionLimitZBack, Concept::CollisionEnabled, Concept::CollisionMargin,
         Concept::CollisionReleaseSmoothing, Concept::ToggleKey, Concept::CycleTrackingModeKey,
         Concept::YawModeKey});
    table.Select(Concept::WorldSpaceYaw).Writable()
        .Select(Concept::RotationEnabled).Writable()
        .Select(Concept::PositionEnabled).Writable();
    table.Select(Concept::CollisionMargin)
        .Comment("How far, in metres, the view is held off a wall when you lean into it.");
    table.Local("Diagnostics", "Verbose", &Config::verbose, cameraunlock::config::BoolCodec(),
                "true: write a detailed trace to the log twice a second, and take a screenshot through\n"
                "the game's own renderer whenever a file named DyingLightHeadTracking.shot appears next\n"
                "to this one. For troubleshooting; it makes the log grow quickly.");
    table.Local("Diagnostics", "IgnoreGameplayGate", &Config::ignore_gameplay_gate,
                cameraunlock::config::BoolCodec(),
                "true: apply the head pose even where the mod would normally stand down: the front end,\n"
                "a pause, a cutscene. It exists to prove the camera hook works on a machine where getting\n"
                "into gameplay is awkward, and it is not a way to have head tracking in menus. Leave it\n"
                "false to play.");
    return table;
}

cameraunlock::config::LegacyImport<Config> MakeLegacyImport() {
    return {&Import, legacy::ReadKeys()};
}

cameraunlock::config::ConfigOwnerOptions<Config> MakeConfigOwnerOptions(const std::wstring& folder,
                                                                        cameraunlock::config::DefaultsFile defaults) {
    const auto wide = [](const char* name) { return std::wstring(name, name + std::char_traits<char>::length(name)); };
    cameraunlock::config::ConfigOwnerOptions<Config> options;
    options.path = folder + wide(kConfigFileName);
    options.legacy_path = folder + wide(kLegacyConfigFileName);
    options.table = MakeConfigTable();
    options.import = MakeLegacyImport();
    options.header.display_name = kConfigDisplayName;
    options.defaults = std::move(defaults);
    return options;
}

}  // namespace DyingLightHeadTracking
