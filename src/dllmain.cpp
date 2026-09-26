#include "camera_hook.h"
#include "config.h"
#include "diagnostics.h"
#include "engine_api.h"
#include "fov_reference.h"
#include "eyex_block.h"
#include "hotkeys.h"
#include "flashlight.h"
#include "hud_crosshair.h"
#include "logging.h"
#include "path_utils.h"
#include "perf_probe.h"
#include "tracking_runtime.h"
#include "version.h"
#include "window_centering.h"
#include "world_query.h"

#include "cameraunlock/config/config_owner.h"
#include "cameraunlock/diagnostics/crash_handler.h"
#include "cameraunlock/tracking/tracking_mode.h"

#include <MinHook.h>
#include <process.h>
#include <windows.h>

#include <exception>
#include <string>

namespace {

using namespace DyingLightHeadTracking;

constexpr const char* kGameExe = "DyingLightGame.exe";
constexpr const char* kEngineDll = "engine_x64_rwdi.dll";
constexpr const char* kGameDll = "gamedll_x64_rwdi.dll";
constexpr const char* kLogFileName = "DyingLightHeadTracking.log";
constexpr const char* kShotTriggerName = "DyingLightHeadTracking.shot";

constexpr int kPollMs = 100;
constexpr int kModuleWaitMs = 60000;
constexpr int kHeartbeatMs = 5000;
constexpr DWORD kInitThreadJoinMs = 2000;

HANDLE g_initThread = nullptr;
HANDLE g_shutdownEvent = nullptr;

TrackingRuntime g_tracking;
Hotkeys g_hotkeys;
bool g_minHookReady = false;

// The one reader and writer of CameraUnlock.ini. Built and loaded on the init thread before the
// hotkeys start, and saved through from the hotkey thread afterwards. Never destroyed: the
// poller thread can still be saving while DLL_PROCESS_DETACH runs at process exit.
cameraunlock::config::ConfigOwner<Config>* g_configOwner = nullptr;

// A save that did not happen has already reached the log through the status sink; the session
// keeps the state the toggle applied. A save that did can carry a line too, naming a row that
// stopped following Defaults.ini.
void LogSave(const cameraunlock::config::ConfigSaveResult& saved) {
    for (const std::string& line : saved.log) Log::Line("%s", line.c_str());
    if (saved.status != cameraunlock::config::ConfigSaveStatus::Saved) {
        Log::Line("WARN: the change applies for this session only.");
    }
}

// Each toggle applies its new state first, then saves it. End is not here: it changes the
// session only, and EnableOnStartup decides the next start.
void CycleTrackingModeAndSave() {
    const cameraunlock::TrackingModeChannels channels =
        cameraunlock::EncodeTrackingMode(g_tracking.CycleTrackingMode());
    LogSave(g_configOwner->Save([channels](Config& c) {
        c.rotation_enabled = channels.rotation_enabled;
        c.position_enabled = channels.position_enabled;
    }));
}

void ToggleYawModeAndSave() {
    const bool worldSpace = g_tracking.ToggleYawMode();
    LogSave(g_configOwner->Save([worldSpace](Config& c) { c.world_space_yaw = worldSpace; }));
}

bool SleepUnlessUnloading(int ms) {
    return WaitForSingleObject(g_shutdownEvent, static_cast<DWORD>(ms)) == WAIT_TIMEOUT;
}

bool WaitForModule(const char* name) {
    for (int waited = 0; waited < kModuleWaitMs; waited += kPollMs) {
        if (GetModuleHandleA(name)) return true;
        if (!SleepUnlessUnloading(kPollMs)) return false;
    }
    Log::Line("ERROR: %s never appeared after %ds. This is not the game process, or the "
              "module has been renamed. Head tracking is inactive here.",
              name, kModuleWaitMs / 1000);
    return false;
}

void OpenSessionLog() {
    const std::wstring logPath = GetModulePathW(kLogFileName);
    if (logPath.empty()) {
        OutputDebugStringW(L"DyingLightHeadTracking: could not resolve its own directory; "
                           L"no log file this session\n");
        return;
    }
    Log::Open(logPath);
    cameraunlock::diagnostics::InstallCrashHandler();
}

class Heartbeat {
public:
    void Tick() {
        const unsigned long long updates = CameraUpdateCount();
        const unsigned long long posed = PosedFrameCount();
        const bool driving = updates != m_lastUpdates;
        if (m_first || driving != m_lastDriving) {
            Log::Line("Camera updates: %s (%llu seen, %llu posed)",
                      driving ? "arriving" : "stopped", updates, posed);
            m_lastDriving = driving;
        }
        m_lastUpdates = updates;

        const bool receiving = g_tracking.IsReceiving();
        if (m_first || receiving != m_lastReceiving) {
            Log::Line("OpenTrack: %s", receiving ? "receiving data" : "no data");
            m_lastReceiving = receiving;
        }

        const bool posing = posed != m_lastPosed;
        const std::string state = DescribeCameraState();
        if (m_first || posing != m_lastPosing || state != m_lastState) {
            Log::Line("Head pose: %s (gate: %s, tracking %s)", posing ? "applied" : "not applied",
                      state.c_str(), g_tracking.IsEnabled() ? "on" : "off");
            m_lastPosing = posing;
            m_lastState = state;
        }
        m_lastPosed = posed;
        m_first = false;

        const std::string perf = perf_probe::TakeWindowReport();
        if (!perf.empty()) {
            Log::Line("%s (gate: %s, tracking %s)", perf.c_str(), state.c_str(),
                      g_tracking.IsEnabled() ? "on" : "off");
        }
    }

private:
    bool m_first = true;
    bool m_lastDriving = false;
    bool m_lastReceiving = false;
    bool m_lastPosing = false;
    std::string m_lastState;
    unsigned long long m_lastUpdates = 0;
    unsigned long long m_lastPosed = 0;
};

unsigned InitThreadBody() {
    OpenSessionLog();
    Log::Line("%s v%s loaded", kModName, kModVersion);

    if (!g_shutdownEvent) {
        Log::Line("ERROR: shutdown event could not be created; staying dormant");
        return 1;
    }
    window_centering::Start(g_shutdownEvent);
    if (!GetModuleHandleA(kGameExe)) {
        Log::Line("WARN: %s is not this process; continuing anyway in case the game exe has "
                  "been renamed", kGameExe);
    }
    if (!WaitForModule(kEngineDll)) return 1;
    if (!WaitForModule(kGameDll)) return 1;

    // The engine exports every symbol this mod needs by name, so nothing is
    // pinned to an address a patch can move. Resolve() failing means a symbol is
    // genuinely gone, and then the mod does nothing at all rather than hooking
    // something it does not recognise.
    if (!engine::Resolve()) {
        Log::Line("Staying dormant: this build of %s is not the one this mod knows how to "
                  "drive. The game runs unmodified.", kEngineDll);
        return 1;
    }
    engine::LogResolution();

    // The engine starts EyeX once, on its own thread. Every EyeX import is stubbed
    // rather than just the initialiser, so if the engine got there first the
    // gaze callbacks still cannot read anything back out.
    eyex_block::Install(GetModuleHandleA(kEngineDll));

    // Only DyingLightGame.exe loads this mod: the ASI loader sits beside it, and nothing else
    // in the game folder imports it, so one process opens this folder's config.
    const std::wstring folder = GetModuleDirectoryW();
    if (folder.empty()) {
        Log::Line("ERROR: the folder this mod was loaded from could not be read, so there is "
                  "nowhere to read the settings from; staying dormant");
        return 1;
    }
    cameraunlock::config::ConfigOwnerOptions<Config> options =
        MakeConfigOwnerOptions(folder, cameraunlock::config::DefaultsFile::PerUser());
    // The log is the only place this mod can tell the player anything.
    options.status_sink = [](const std::string& message) { Log::Line("WARN: %s", message.c_str()); };
    g_configOwner = new cameraunlock::config::ConfigOwner<Config>(std::move(options));

    const cameraunlock::config::ConfigLoadResult<Config> loaded = g_configOwner->Load();
    for (const std::string& line : loaded.log) Log::Line("%s", line.c_str());
    Log::Line("Config: %s", cameraunlock::config::ConfigLoadStatusName(loaded.status));
    // The build DyingLightHeadTracking.ini was written for refused it and did not start, so this
    // one does the same until the player fixes it.
    if (loaded.status == cameraunlock::config::ConfigLoadStatus::LegacyRefused) {
        Log::Line("ERROR: Config load failed");
        return 1;
    }
    const Config& cfg = loaded.config;
    Log::Line("Config: port=%d enabled=%d smoothing local %.2f / remote %.2f rotation=%d "
              "position=%d worldyaw=%d collision=%d margin %.2f verbose=%d",
              cfg.udp_port, cfg.enable_on_startup, static_cast<double>(cfg.local_smoothing),
              static_cast<double>(cfg.remote_smoothing), cfg.rotation_enabled, cfg.position_enabled,
              cfg.world_space_yaw, cfg.collision_enabled, static_cast<double>(cfg.lean_clamp.skin),
              cfg.verbose);

    const MH_STATUS mh = MH_Initialize();
    if (mh != MH_OK) {
        Log::Line("ERROR: MinHook initialisation failed: %s", MH_StatusToString(mh));
        return 1;
    }
    g_minHookReady = true;

    g_tracking.Start(cfg);
    if (!g_hotkeys.Start(cfg, [] { g_tracking.ToggleEnabled(); }, [] { CycleTrackingModeAndSave(); },
                         [] { ToggleYawModeAndSave(); })) {
        g_tracking.Stop();
        return 1;
    }

    diagnostics::Start(cfg.verbose, GetModulePathW(kShotTriggerName));

    world_query::Install();

    if (!InstallCameraHook(g_tracking, cfg)) {
        world_query::Remove();
        diagnostics::Stop();
        g_hotkeys.Stop();
        g_tracking.Stop();
        return 1;
    }
    if (cfg.ignore_gameplay_gate) {
        Log::Line("WARN: Diagnostics.IgnoreGameplayGate is on, so the head pose is applied in "
                  "menus and cutscenes too. Turn it off to play.");
    }
    // Without these the head pose still applies; only the crosshair stays at
    // centre, zooms go uncompensated, or the torch stays on the aim.
    hud_crosshair::Install();
    fov_reference::Install();
    flashlight::Install();

    Log::Line("%s ready", kModName);
    Heartbeat heartbeat;
    do {
        heartbeat.Tick();
    } while (SleepUnlessUnloading(kHeartbeatMs));
    return 0;
}

// A __stdcall thread procedure, where an escaping exception is std::terminate: the game dying
// outright with the log stopping mid-startup. The UDP receiver and the hotkey poller each
// construct a std::thread, which throws when the process cannot spawn one, and the config owner
// allocates and throws for a table it cannot render.
unsigned __stdcall InitThread(void*) {
    try {
        return InitThreadBody();
    } catch (const std::exception& e) {
        Log::Line("ERROR: startup stopped: %s. Head tracking is inactive this session.", e.what());
        return 1;
    }
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            g_shutdownEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            g_initThread =
                reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, InitThread, nullptr, 0, nullptr));
            if (!g_initThread) {
                OutputDebugStringW(L"DyingLightHeadTracking: could not start its init thread; "
                                   L"the mod is doing nothing this session\n");
            }
            break;

        case DLL_PROCESS_DETACH:
            if (g_shutdownEvent) SetEvent(g_shutdownEvent);
            // Process exit: every other thread is already gone and may have died
            // holding a lock, so joining or unpatching here could hang the game on
            // the way out.
            if (lpReserved != nullptr) {
                Log::EmergencyLine("%s unloading (process exit)", kModName);
                break;
            }
            if (g_initThread) {
                if (WaitForSingleObject(g_initThread, kInitThreadJoinMs) != WAIT_OBJECT_0) {
                    Log::Line("WARN: the init thread did not exit within 2s; unpatching anyway");
                }
                CloseHandle(g_initThread);
                g_initThread = nullptr;
            }
            window_centering::Stop();
            eyex_block::Remove();
            diagnostics::Stop();
            hud_crosshair::Remove();
            flashlight::Remove();
            RemoveCameraHook();
            world_query::Remove();
            if (g_minHookReady) MH_Uninitialize();
            g_hotkeys.Stop();
            g_tracking.Stop();
            Log::Line("%s unloading", kModName);
            Log::Close();
            break;
    }
    return TRUE;
}
