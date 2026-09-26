// The config differential test (convert-a-mod-to-the-canonical-config, section 5). Every input
// is read two ways:
//
//   oracle     the reader of the newest published build, v0.1.0, with the core sources it
//              compiled at its pin 8d18bb3 (oracle_adapter.h)
//   import     the frozen reader in src/legacy_config/
//
// Comparison 1, oracle against import, on every input: load status, every field both read
// (floats bit for bit), the startup state, and which actions every key press fires under every
// set of held modifiers. The frozen reader's files and the core sources it compiles hash-equal
// v0.1.0's (provenance.txt), so it finds no difference.
//
// Inputs: no file, an empty file, the file v0.1.0 shipped (its installer ZIP and its launcher
// seed carried the same bytes), v0.1.0's first-run output, and core's corpus over each of the
// two.

#include "legacy_config/legacy_config.h"
#include "oracle_adapter.h"

#include "cameraunlock/config/testing/ini_mutations.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace DyingLightHeadTracking;

namespace {

constexpr const char* kFileName = "DyingLightHeadTracking.ini";

int g_failures = 0;
int g_checks = 0;

void Check(bool cond, const std::string& what) {
    ++g_checks;
    if (!cond) {
        if (g_failures < 200) std::printf("  FAIL: %s\n", what.c_str());
        ++g_failures;
    }
}

bool SameBits(float a, float b) { return std::memcmp(&a, &b, sizeof a) == 0; }

std::string ReadBytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteBytes(const fs::path& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out) throw std::runtime_error("could not write " + path.string());
}

struct Listing {
    std::vector<std::pair<std::string, std::string>> files;
    bool operator==(const Listing& o) const { return files == o.files; }
};

Listing List(const fs::path& dir) {
    Listing l;
    for (const auto& e : fs::directory_iterator(dir)) {
        l.files.emplace_back(e.path().filename().string(), ReadBytes(e.path()));
    }
    std::sort(l.files.begin(), l.files.end());
    return l;
}

void SetReadOnly(const fs::path& path, bool readOnly) {
    const DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) throw std::runtime_error("no attributes for " + path.string());
    const DWORD next = readOnly ? (attrs | FILE_ATTRIBUTE_READONLY) : (attrs & ~FILE_ATTRIBUTE_READONLY);
    if (!SetFileAttributesW(path.c_str(), next)) throw std::runtime_error("could not set attributes on " + path.string());
}

struct Input {
    std::string name;
    std::optional<std::string> bytes;  // nullopt: no file
};

// Startup state as v0.1.0's TrackingRuntime::Start derives it from the config: enabled from
// enabled_on_startup, RotationAndPosition when position_enabled else RotationOnly, the yaw mode
// from world_space_yaw and crosshair compensation from show_reticle.
struct Startup {
    bool enabled;
    int mode;  // cameraunlock::TrackingMode: 0 rotation and position, 1 rotation only
    bool world_space_yaw;
    bool reticle;
};

template <class C>
Startup StartupOf(const C& c) {
    return {c.enabled_on_startup, c.position_enabled ? 0 : 1, c.world_space_yaw, c.show_reticle};
}

bool SameStartup(const Startup& a, const Startup& b) {
    return a.enabled == b.enabled && a.mode == b.mode && a.world_space_yaw == b.world_space_yaw &&
           a.reticle == b.reticle;
}

template <class C>
dlht_oracle_view::HotkeyView KeysOf(const C& c) {
    return {c.vk_toggle,    c.vk_cycle_mode,    c.vk_yaw_mode,    c.vk_reticle,
            c.chord_toggle, c.chord_cycle_mode, c.chord_yaw_mode, c.chord_reticle};
}

// The first press whose fired actions differ, for the failure message.
std::string FirstFireDifference(const dlht_oracle_view::FireTable& expected, const dlht_oracle_view::FireTable& got) {
    for (std::size_t i = 0; i < expected.size() && i < got.size(); ++i) {
        if (expected[i] != got[i]) {
            char text[160];
            std::snprintf(text, sizeof text, "key 0x%02X held %d fires %d/%d/%d/%d, not %d/%d/%d/%d",
                          static_cast<int>(i / dlht_oracle_view::kHeldStates) + dlht_oracle_view::kFirstKey,
                          static_cast<int>(i % dlht_oracle_view::kHeldStates), got[i][0], got[i][1], got[i][2],
                          got[i][3], expected[i][0], expected[i][1], expected[i][2], expected[i][3]);
            return text;
        }
    }
    return expected.size() == got.size() ? "none" : "the tables differ in size";
}

// Every field the import reads, against the oracle's field of the same name.
std::vector<std::string> FieldDifferences(const dlht_oracle_view::OracleConfig& o, const legacy::Config& i) {
    std::vector<std::string> d;
    auto b = [&d](const char* n, bool x, bool y) { if (x != y) d.push_back(n); };
    auto f = [&d](const char* n, float x, float y) { if (!SameBits(x, y)) d.push_back(n); };
    auto n = [&d](const char* name, long long x, long long y) { if (x != y) d.push_back(name); };
    b("enabled_on_startup", o.enabled_on_startup, i.enabled_on_startup);
    n("udp_port", o.udp_port, i.udp_port);
    n("data_freshness_ms", o.data_freshness_ms, i.data_freshness_ms);
    b("world_space_yaw", o.world_space_yaw, i.world_space_yaw);
    b("show_reticle", o.show_reticle, i.show_reticle);
    f("local_smoothing", o.local_smoothing, i.local_smoothing);
    f("remote_smoothing", o.remote_smoothing, i.remote_smoothing);
    b("position_enabled", o.position_enabled, i.position_enabled);
    f("pos_limit_x", o.pos_limit_x, i.pos_limit_x);
    f("pos_limit_y", o.pos_limit_y, i.pos_limit_y);
    f("pos_limit_y_down", o.pos_limit_y_down, i.pos_limit_y_down);
    f("pos_limit_z", o.pos_limit_z, i.pos_limit_z);
    f("pos_limit_z_back", o.pos_limit_z_back, i.pos_limit_z_back);
    b("verbose", o.verbose, i.verbose);
    b("ignore_gameplay_gate", o.ignore_gameplay_gate, i.ignore_gameplay_gate);
    b("collision_enabled", o.collision_enabled, i.collision_enabled);
    f("collision_radius", o.collision_radius, i.collision_radius);
    f("collision_release_smoothing", o.collision_release_smoothing, i.collision_release_smoothing);
    n("vk_toggle", o.vk_toggle, i.vk_toggle);
    n("vk_cycle_mode", o.vk_cycle_mode, i.vk_cycle_mode);
    n("vk_yaw_mode", o.vk_yaw_mode, i.vk_yaw_mode);
    n("vk_reticle", o.vk_reticle, i.vk_reticle);
    b("chord_toggle", o.chord_toggle, i.chord_toggle);
    b("chord_cycle_mode", o.chord_cycle_mode, i.chord_cycle_mode);
    b("chord_yaw_mode", o.chord_yaw_mode, i.chord_yaw_mode);
    b("chord_reticle", o.chord_reticle, i.chord_reticle);
    return d;
}

std::string Join(const std::vector<std::string>& v) {
    std::string s;
    for (const std::string& x : v) s += (s.empty() ? "" : ", ") + x;
    return s;
}

// The corpus descriptor of every key the frozen reader reads.
std::vector<cameraunlock::config::testing::MutationKey> MutationKeys() {
    using cameraunlock::config::testing::ChordSwitch;
    using cameraunlock::config::testing::MutationKey;
    auto plain = [](const char* s, const char* k, const char* alt, std::vector<std::string> oor = {}) {
        MutationKey m;
        m.section = s;
        m.key = k;
        m.alternate = alt;
        m.out_of_range = std::move(oor);
        return m;
    };
    auto hotkey = [](const char* k, const char* alt, const char* chord) {
        MutationKey m;
        m.section = "Hotkeys";
        m.key = k;
        m.alternate = alt;
        m.out_of_range = {"0xFF"};
        m.hotkey = true;
        m.chords.push_back(ChordSwitch{"Hotkeys", chord, "1", "0"});
        return m;
    };
    return {
        plain("General", "EnableOnStartup", "false"),
        plain("General", "Port", "4243", {"1023", "65536"}),
        plain("General", "DataFreshnessMs", "250", {"0"}),
        plain("General", "WorldSpaceYaw", "false"),
        plain("General", "ShowReticle", "false"),
        plain("Smoothing", "LocalSmoothing", "0.3", {"-0.5", "1.5"}),
        plain("Smoothing", "RemoteSmoothing", "0.3", {"-0.5", "1.5"}),
        plain("Position", "Enabled", "false"),
        plain("Position", "LimitX", "0.5", {"-0.3"}),
        plain("Position", "LimitY", "0.5", {"-0.2"}),
        plain("Position", "LimitYDown", "0.5", {"-0.2"}),
        plain("Position", "LimitZ", "0.5", {"-0.4"}),
        plain("Position", "LimitZBack", "0.2", {"-0.1"}),
        plain("Diagnostics", "Verbose", "true"),
        plain("Diagnostics", "IgnoreGameplayGate", "true"),
        plain("Collision", "CollisionEnabled", "true"),
        plain("Collision", "CollisionRadius", "0.25", {"0.01", "0.6"}),
        plain("Collision", "CollisionReleaseSmoothing", "0.5", {"-0.5", "1.5"}),
        hotkey("Toggle", "0x70", "ChordToggle"),
        hotkey("CycleMode", "0x71", "ChordCycleMode"),
        hotkey("YawMode", "0x72", "ChordYawMode"),
        hotkey("Reticle", "0x73", "ChordReticle"),
        plain("Hotkeys", "ChordToggle", "0"),
        plain("Hotkeys", "ChordCycleMode", "0"),
        plain("Hotkeys", "ChordYawMode", "0"),
        plain("Hotkeys", "ChordReticle", "0"),
    };
}

// One folder per reading under a root of this process's own, emptied before each input so the
// test never holds more than one input's files.
class Scratch {
public:
    Scratch() {
        root_ = fs::temp_directory_path() / ("dlht-config-differential-" + std::to_string(GetCurrentProcessId()));
        Remove(root_);
        fs::create_directories(root_);
    }
    ~Scratch() { Remove(root_); }
    Scratch(const Scratch&) = delete;
    Scratch& operator=(const Scratch&) = delete;

    // root/leaf, created empty.
    fs::path Clean(const std::string& leaf) {
        const fs::path dir = root_ / leaf;
        Remove(dir);
        fs::create_directories(dir);
        return dir;
    }

private:
    // Read-only files included, which remove_all will not delete.
    static void Remove(const fs::path& dir) {
        std::error_code ec;
        if (!fs::exists(dir, ec)) return;
        for (const auto& e : fs::recursive_directory_iterator(dir, ec)) {
            if (e.is_regular_file()) SetFileAttributesW(e.path().c_str(), FILE_ATTRIBUTE_NORMAL);
        }
        fs::remove_all(dir, ec);
        if (ec) throw std::runtime_error("could not empty " + dir.string() + ": " + ec.message());
    }

    fs::path root_;
};

fs::path Place(const fs::path& dir, const Input& input) {
    const fs::path file = dir / kFileName;
    if (input.bytes) WriteBytes(file, *input.bytes);
    return file;
}

struct ImportRun {
    legacy::Config config;
    legacy::ReadResult result;
};

// The import on a read-only copy of the input, which must leave its folder as it found it.
ImportRun RunImport(Scratch& scratch, const Input& input) {
    const fs::path dir = scratch.Clean("import");
    const fs::path file = Place(dir, input);
    if (input.bytes) SetReadOnly(file, true);
    const Listing before = List(dir);
    ImportRun run;
    run.result = run.config.Read(file.string().c_str());
    Check(List(dir) == before, input.name + ": the import changed its folder");
    return run;
}

ImportRun Comparison1(Scratch& scratch, const Input& input) {
    const fs::path odir = scratch.Clean("oracle");
    const dlht_oracle_view::OracleResult oracle = dlht_oracle_view::RunOracle(Place(odir, input).string());
    const ImportRun import = RunImport(scratch, input);

    const bool importUsable = import.result.status != legacy::ReadStatus::Refused;
    Check(oracle.loaded == importUsable, input.name + ": load status differs (oracle " +
                                             (oracle.loaded ? "loaded" : "refused") + ")");
    Check(input.bytes.has_value() || import.result.status == legacy::ReadStatus::Absent,
          input.name + ": no file is not Absent");
    if (oracle.loaded && importUsable) {
        const std::vector<std::string> fields = FieldDifferences(oracle.config, import.config);
        Check(fields.empty(), input.name + ": fields differ: " + Join(fields));
        Check(SameStartup(StartupOf(oracle.config), StartupOf(import.config)), input.name + ": startup state differs");
        const dlht_oracle_view::FireTable oracleFires = dlht_oracle_view::OracleFires(KeysOf(oracle.config));
        const dlht_oracle_view::FireTable importFires = dlht_oracle_view::OracleFires(KeysOf(import.config));
        Check(oracleFires == importFires,
              input.name + ": hotkeys fire differently: " + FirstFireDifference(oracleFires, importFires));
    }
    return import;
}

std::vector<Input> Inputs(const std::string& shipped, const std::string& firstRun) {
    using cameraunlock::config::testing::GenerateIniMutations;
    std::vector<Input> inputs;
    inputs.push_back({"no file", std::nullopt});
    inputs.push_back({"empty file", std::string()});
    inputs.push_back({"v0.1.0 shipped", shipped});
    inputs.push_back({"v0.1.0 first-run output", firstRun});
    for (auto& m : GenerateIniMutations(shipped, legacy::ReadKeys(), MutationKeys())) {
        inputs.push_back({"corpus over shipped: " + m.name, std::move(m.bytes)});
    }
    for (auto& m : GenerateIniMutations(firstRun, legacy::ReadKeys(), MutationKeys())) {
        inputs.push_back({"corpus over first run: " + m.name, std::move(m.bytes)});
    }
    return inputs;
}

}  // namespace

int main() {
    try {
        Scratch scratch;

        const std::string shipped = ReadBytes(fs::path(DLHT_DIFFERENTIAL_DATA) / "v0.1.0-shipped.ini");
        Check(!shipped.empty(), "data/v0.1.0-shipped.ini is missing");

        // v0.1.0's first-run output, committed once as test data, is what the oracle still
        // writes for a missing file.
        const std::string firstRun = ReadBytes(fs::path(DLHT_DIFFERENTIAL_DATA) / "v0.1.0-first-run.ini");
        Check(!firstRun.empty(), "data/v0.1.0-first-run.ini is missing");
        {
            const fs::path file = scratch.Clean("first-run") / kFileName;
            Check(dlht_oracle_view::RunOracle(file.string()).loaded, "the oracle did not load its own first run");
            Check(ReadBytes(file) == firstRun, "the oracle's first-run output differs from data/v0.1.0-first-run.ini");
        }

        const std::vector<Input> inputs = Inputs(shipped, firstRun);
        std::printf("%zu inputs\n", inputs.size());
        std::printf("comparison 1, the oracle (v0.1.0) against the import\n");
        int refused = 0;
        for (const Input& input : inputs) {
            const ImportRun import = Comparison1(scratch, input);
            if (import.result.status == legacy::ReadStatus::Refused) ++refused;
        }
        std::printf("  %d refused as v0.1.0 refused them\n", refused);
        Check(refused > 0, "no input is refused");
    } catch (const std::exception& e) {
        std::printf("  FAIL: threw: %s\n", e.what());
        ++g_failures;
    }

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
