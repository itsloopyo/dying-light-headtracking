// CameraUnlock.ini and what the conversion moved into code: the committed file is the table's
// fresh render, the defaults v0.1.0 ran on map to the defaults, a first start creates the
// committed file, the toggles save only their own lines and leave Defaults.ini and
// DyingLightHeadTracking.ini alone, End's row cannot be saved, and the import finds
// DyingLightHeadTracking.ini by the ANSI path v0.1.0 opened it by.
//
// `--render-config <path>` writes the committed file instead (pixi run render-config).

#include "config.h"
#include "path_utils.h"

#include "legacy_config/legacy_config.h"

#include "cameraunlock/tracking/tracking_mode.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace DyingLightHeadTracking;

namespace {

namespace cfg = cameraunlock::config;
namespace fs = std::filesystem;

int g_failures = 0;

void Check(bool ok, const std::string& what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what.c_str());
        ++g_failures;
    }
}

std::string ReadBytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + path.string());
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteBytes(const fs::path& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("cannot write " + path.string());
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

// The file a first start creates.
std::string Rendered() {
    return cfg::RenderCanonicalFresh(MakeConfigTable(), {kConfigDisplayName});
}

std::string Committed() {
    return ReadBytes(fs::path(DLHT_COMMITTED_CONFIG));
}

void TestCommittedConfigIsRendered() {
    Check(Committed() == Rendered(), "DyingLightHeadTracking.ini is the table's fresh render (pixi run render-config)");
}

// A scratch game folder, and a Defaults.ini of its own beside it that the first load creates.
struct Scratch {
    fs::path root;
    fs::path game;
    fs::path defaults;

    explicit Scratch(const std::wstring& gameFolder) {
        wchar_t temp[MAX_PATH];
        GetTempPathW(MAX_PATH, temp);
        root = fs::path(temp) / (L"dlht-config-tests-" + std::to_wstring(GetCurrentProcessId()));
        game = root / gameFolder;
        fs::remove_all(game);
        fs::create_directories(game);
        fs::create_directories(root / L"user");
        defaults = root / L"user" / L"CameraUnlock" / L"Defaults.ini";
    }
    ~Scratch() {
        std::error_code ignored;
        fs::remove_all(root, ignored);
    }
    Scratch(const Scratch&) = delete;
    Scratch& operator=(const Scratch&) = delete;

    cfg::ConfigOwnerOptions<Config> Options() const {
        return MakeConfigOwnerOptions(game.wstring() + L"\\", cfg::DefaultsFile::At(defaults.wstring()));
    }
    fs::path ConfigPath() const { return game / kConfigFileName; }
    fs::path LegacyPath() const { return game / kLegacyConfigFileName; }
};

std::vector<std::string> Listing(const fs::path& dir) {
    std::vector<std::string> names;
    for (const fs::directory_entry& entry : fs::directory_iterator(dir)) names.push_back(entry.path().filename().string());
    std::sort(names.begin(), names.end());
    return names;
}

std::string AllValues(const Config& c) {
    return cfg::RenderCanonical(MakeConfigTable(), c, {kConfigDisplayName});
}

// With neither file there, the first start creates CameraUnlock.ini as the committed file and
// Defaults.ini with the built-in values, and no DyingLightHeadTracking.ini.
void TestFirstStartCreatesTheCommittedFile() {
    const Scratch s(L"first-start");
    const auto loaded = cfg::ConfigOwner<Config>(s.Options()).Load();
    Check(loaded.status == cfg::ConfigLoadStatus::Created, "a first start with no file is Created");
    Check(ReadBytes(s.ConfigPath()) == Committed(), "a first start creates DyingLightHeadTracking.ini's bytes as CameraUnlock.ini");
    Check(Listing(s.game) == std::vector<std::string>{"CameraUnlock.ini"}, "a first start creates CameraUnlock.ini and nothing else");
    Check(fs::exists(s.defaults), "a first start creates Defaults.ini");
    const auto table = MakeConfigTable();
    Check(AllValues(loaded.config) == AllValues(table.defaults()), "a first start runs on the built-in values");
    Check(loaded.config.collision_enabled, "the lean clamp starts on");
    Check(loaded.config.lean_clamp.skin == kCollisionMarginMetres, "the lean clamp keeps this game's 0.15 m margin");
}

// A fresh install and an upgrade from v0.1.0's defaults start the same: the map of the frozen
// defaults holds every row at the table's default. The lean clamp shipped off pending
// verification and now takes its default, and the crosshair switch and key are gone.
void TestLegacyDefaultsMapToTheDefaults() {
    wchar_t temp[MAX_PATH];
    GetTempPathW(MAX_PATH, temp);
    const std::wstring missing = std::wstring(temp) + L"dlht-no-such-folder\\" + L"DyingLightHeadTracking.ini";
    const auto table = MakeConfigTable();
    Config mapped = table.defaults();
    const cfg::ImportResult result = MakeLegacyImport().run(cfg::LegacyInput{missing, LegacyAnsiPath(missing), false}, mapped);
    Check(result.status == cfg::ImportStatus::Absent, "no file imports as Absent");
    const auto dropped = [&result](cfg::DropRule rule, const char* section, const char* key, const char* value) {
        for (const cfg::DroppedValue& d : result.dropped) {
            if (d.rule == rule && d.section == section && d.key == key && d.value == value) return true;
        }
        return false;
    };
    Check(result.dropped.size() == 3 &&
              dropped(cfg::DropRule::FollowsDefault, "Collision", "CollisionEnabled", "0") &&
              dropped(cfg::DropRule::Reticle, "Hotkeys", "Reticle", "0x2D") &&
              dropped(cfg::DropRule::Reticle, "Hotkeys", "ChordReticle", "1"),
          "the old defaults drop [Collision] CollisionEnabled=0, which follows the default now, and the "
          "crosshair key and its chord");
    Check(result.pose_shaping.empty(), "v0.1.0 read no sensitivity, inversion or scale");
    Check(AllValues(mapped) == AllValues(table.defaults()), "the old defaults map to the defaults");
    Check(mapped.toggle_key_name == "End, Ctrl+Shift+Y" && mapped.cycle_tracking_mode_key_name == "PageUp, Ctrl+Shift+G" &&
              mapped.yaw_mode_key_name == "PageDown, Ctrl+Shift+H",
          "the old hotkeys and their chord switches become the fleet's key lists");
    Check(legacy::Config{}.collision_radius == kCollisionMarginMetres, "the margin is the one v0.1.0 shipped");
}

std::vector<std::string> Lines(const std::string& bytes) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < bytes.size()) {
        const size_t end = bytes.find("\r\n", start);
        lines.push_back(bytes.substr(start, end - start));
        start = end + 2;
    }
    return lines;
}

// The lines of `after` that differ from `before`, which must have as many lines.
std::vector<std::string> ChangedLines(const std::string& before, const std::string& after) {
    const std::vector<std::string> a = Lines(before);
    const std::vector<std::string> b = Lines(after);
    if (a.size() != b.size()) return {"a line was added or removed"};
    std::vector<std::string> changed;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) changed.push_back(b[i]);
    }
    return changed;
}

bool Contains(const std::vector<std::string>& lines, const std::string& text) {
    for (const std::string& line : lines) {
        if (line.find(text) != std::string::npos) return true;
    }
    return false;
}

// A save changes the lines of its rows and no other byte, writes a value over default, and
// touches neither Defaults.ini nor DyingLightHeadTracking.ini; the yaw mode and the tracking mode
// persist, and End's row cannot be saved at all.
void TestTogglesSave() {
    const Scratch s(L"save");
    const std::string committed = Committed();
    const std::string legacyBytes = "[General]\r\nEnableOnStartup=0\r\n";
    WriteBytes(s.ConfigPath(), committed);
    WriteBytes(s.LegacyPath(), legacyBytes);

    {
        cfg::ConfigOwner<Config> owner(s.Options());
        const auto loaded = owner.Load();
        Check(loaded.status == cfg::ConfigLoadStatus::Canonical, "the committed file loads as canonical");
        Check(loaded.config.enable_on_startup, "DyingLightHeadTracking.ini is not read while CameraUnlock.ini exists");
        Check(Contains(loaded.log, "is left as it was and is not read"),
              "the log says DyingLightHeadTracking.ini is not read while CameraUnlock.ini exists");
        const std::string defaultsBefore = ReadBytes(s.defaults);

        const cfg::ConfigSaveResult yaw = owner.Save([](Config& c) { c.world_space_yaw = false; });
        Check(yaw.status == cfg::ConfigSaveStatus::Saved, "the yaw mode saves");
        Check(Contains(yaw.log, "WorldSpaceYaw=false is now set for this game, and no longer follows Defaults.ini"),
              "the yaw save says WorldSpaceYaw stopped following Defaults.ini");
        const std::string afterYaw = ReadBytes(s.ConfigPath());
        Check(ChangedLines(committed, afterYaw) == std::vector<std::string>{"WorldSpaceYaw=false"},
              "saving the yaw mode writes its value over default and changes nothing else");

        const auto rotationOnly = cameraunlock::EncodeTrackingMode(cameraunlock::TrackingMode::RotationOnly);
        Check(owner.Save([rotationOnly](Config& c) {
                  c.rotation_enabled = rotationOnly.rotation_enabled;
                  c.position_enabled = rotationOnly.position_enabled;
              }).status == cfg::ConfigSaveStatus::Saved,
              "the tracking mode saves");
        const std::string afterRotationOnly = ReadBytes(s.ConfigPath());
        Check(ChangedLines(afterYaw, afterRotationOnly) == std::vector<std::string>{"RotationEnabled=true", "PositionEnabled=false"},
              "saving rotation only writes the mode pair over default and changes nothing else");

        const auto positionOnly = cameraunlock::EncodeTrackingMode(cameraunlock::TrackingMode::PositionOnly);
        Check(owner.Save([positionOnly](Config& c) {
                  c.rotation_enabled = positionOnly.rotation_enabled;
                  c.position_enabled = positionOnly.position_enabled;
              }).status == cfg::ConfigSaveStatus::Saved,
              "the third tracking mode saves");
        const std::string afterPositionOnly = ReadBytes(s.ConfigPath());
        Check(ChangedLines(afterRotationOnly, afterPositionOnly) ==
                  std::vector<std::string>{"RotationEnabled=false", "PositionEnabled=true"},
              "saving position only changes the mode pair and nothing else");

        Check(owner.Save([](Config&) {}).status == cfg::ConfigSaveStatus::Saved, "an empty save succeeds");
        Check(ReadBytes(s.ConfigPath()) == afterPositionOnly, "an empty save writes nothing");

        bool refused = false;
        try {
            owner.Save([](Config& c) { c.enable_on_startup = false; });
        } catch (const std::logic_error&) {
            refused = true;
        }
        Check(refused, "EnableOnStartup is not Writable, so the End toggle cannot persist");
        Check(ReadBytes(s.ConfigPath()) == afterPositionOnly, "a refused save writes nothing");

        Check(ReadBytes(s.defaults) == defaultsBefore, "saving leaves Defaults.ini as it was");
        Check(ReadBytes(s.LegacyPath()) == legacyBytes, "saving leaves DyingLightHeadTracking.ini as it was");
    }

    const auto again = cfg::ConfigOwner<Config>(s.Options()).Load();
    Check(again.status == cfg::ConfigLoadStatus::Canonical && again.diagnostics.empty() && !again.config.world_space_yaw &&
              !again.config.rotation_enabled && again.config.position_enabled && again.config.enable_on_startup,
          "the saved yaw and tracking mode come back at the next start");
    Check((Listing(s.game) == std::vector<std::string>{"CameraUnlock.ini", kLegacyConfigFileName}),
          "the game folder holds CameraUnlock.ini and DyingLightHeadTracking.ini and nothing else");
}

// v0.1.0 opened its file by an ANSI path it built itself, falling back to the
// folder's 8.3 short name where the ANSI codepage cannot hold the folder's name, and did not
// start where neither gave a path. The import has to find the file the same way, or a player
// in such a folder would have their settings replaced with the defaults.
void TestAFolderTheCodepageCannotNameImportsAsThePublishedBuildReadIt() {
    const Scratch s(L"dlht-\x4E2D");
    WriteBytes(s.LegacyPath(), "[General]\r\nPort=5000\r\n");
    const std::string ansi = LegacyAnsiPath(s.LegacyPath().wstring());
    const auto loaded = cfg::ConfigOwner<Config>(s.Options()).Load();
    if (ansi.empty()) {
        std::printf("note: this volume gives the scratch folder no short name\n");
        Check(loaded.status == cfg::ConfigLoadStatus::LegacyRefused,
              "with no ANSI path the file is not refused as v0.1.0 refused it");
        return;
    }
    Check(GetFileAttributesA(ansi.c_str()) != INVALID_FILE_ATTRIBUTES, "the ANSI path names no file");
    Check(loaded.status == cfg::ConfigLoadStatus::Migrated, "the file is not Migrated");
    Check(loaded.config.udp_port == 5000, "the file was not read through its ANSI path");
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 3 && std::strcmp(argv[1], "--render-config") == 0) {
            WriteBytes(argv[2], Rendered());
            return 0;
        }
        if (argc != 1) {
            std::printf("usage: %s [--render-config <path>]\n", argv[0]);
            return 2;
        }

        TestCommittedConfigIsRendered();
        TestLegacyDefaultsMapToTheDefaults();
        TestFirstStartCreatesTheCommittedFile();
        TestTogglesSave();
        TestAFolderTheCodepageCannotNameImportsAsThePublishedBuildReadIt();
    } catch (const std::exception& e) {
        std::printf("FAIL: %s\n", e.what());
        return 1;
    }

    if (g_failures == 0) {
        std::printf("config tests: all passed\n");
        return 0;
    }
    std::printf("config tests: %d failure(s)\n", g_failures);
    return 1;
}
