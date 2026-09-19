#include "diagnostics.h"

#include "engine_api.h"
#include "logging.h"

#include <windows.h>

#include <atomic>
#include <cmath>
#include <mutex>
#include <thread>

namespace DyingLightHeadTracking::diagnostics {

namespace {

constexpr int kTraceIntervalMs = 500;
constexpr int kTriggerPollMs = 200;

std::atomic<bool> g_verbose{false};
std::atomic<bool> g_running{false};
std::wstring g_triggerPath;
HANDLE g_stopEvent = nullptr;
std::thread g_thread;

std::mutex g_mutex;
FrameTrace g_latest;
bool g_haveFrame = false;

void TraceLine() {
    FrameTrace t;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_haveFrame) return;
        t = g_latest;
    }

    Log::Line("trace gate=%s%s posed=%d | pose yaw %.2f pitch %.2f roll %.2f | lean R %.3f U %.3f F %.3f",
              t.gate, t.multiplayer ? " (networked)" : "", t.posed ? 1 : 0,
              static_cast<double>(t.pose.yaw_right), static_cast<double>(t.pose.pitch_up),
              static_cast<double>(t.pose.roll_ccw), static_cast<double>(t.pose.right),
              static_cast<double>(t.pose.up), static_cast<double>(t.pose.forward));
    Log::Line("trace clean pos (%.2f %.2f %.2f) fwd (%.3f %.3f %.3f) | rendered pos (%.2f %.2f %.2f) "
              "fwd (%.3f %.3f %.3f)",
              static_cast<double>(t.clean.pos.x), static_cast<double>(t.clean.pos.y),
              static_cast<double>(t.clean.pos.z), static_cast<double>(t.clean.forward.x),
              static_cast<double>(t.clean.forward.y), static_cast<double>(t.clean.forward.z),
              static_cast<double>(t.rendered.pos.x), static_cast<double>(t.rendered.pos.y),
              static_cast<double>(t.rendered.pos.z), static_cast<double>(t.rendered.forward.x),
              static_cast<double>(t.rendered.forward.y), static_cast<double>(t.rendered.forward.z));
    Log::Line("trace aim queried=%d blocked=%d dist %.2f point (%.2f %.2f %.2f) | reticle vis=%d "
              "ndc (%.4f %.4f) | tan half %.4f %.4f | lean contact=%d queryfail=%d",
              t.aim_queried ? 1 : 0, t.aim_blocked ? 1 : 0, static_cast<double>(t.aim_distance),
              static_cast<double>(t.aim_point.x), static_cast<double>(t.aim_point.y),
              static_cast<double>(t.aim_point.z), t.reticle_visible ? 1 : 0,
              static_cast<double>(t.reticle_ndc_x), static_cast<double>(t.reticle_ndc_y),
              static_cast<double>(t.tan_half_h), static_cast<double>(t.tan_half_v),
              t.lean_contact ? 1 : 0, t.lean_query_failed ? 1 : 0);
    if (t.engine_check_valid) {
        Log::Line("trace engine says that aim point rendered at ndc (%.4f %.4f); the mod put the "
                  "reticle at (%.4f %.4f), delta (%.4f %.4f)",
                  static_cast<double>(t.engine_ndc_x), static_cast<double>(t.engine_ndc_y),
                  static_cast<double>(t.reticle_ndc_x), static_cast<double>(t.reticle_ndc_y),
                  static_cast<double>(t.engine_ndc_x - t.reticle_ndc_x),
                  static_cast<double>(t.engine_ndc_y - t.reticle_ndc_y));
    }
}

void Worker() {
    int sinceTrace = 0;
    while (WaitForSingleObject(g_stopEvent, kTriggerPollMs) == WAIT_TIMEOUT) {
        sinceTrace += kTriggerPollMs;
        if (sinceTrace >= kTraceIntervalMs) {
            sinceTrace = 0;
            TraceLine();
        }
        if (!g_triggerPath.empty() &&
            GetFileAttributesW(g_triggerPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            // A trigger that cannot be removed (read-only, or a directory of that
            // name) would fire a screenshot every poll until the disk filled.
            if (!DeleteFileW(g_triggerPath.c_str())) {
                Log::Line("WARN: could not delete %ls (error %lu); screenshot trigger disabled "
                          "for this session", g_triggerPath.c_str(), GetLastError());
                g_triggerPath.clear();
                continue;
            }
            if (engine::HasScreenshot()) {
                engine::TakeScreenshot();
                Log::Line("screenshot requested; the engine writes it under out\\ScreenShots");
            } else {
                Log::Line("screenshot requested but IGame::TakeScreenshot is unavailable");
            }
        }
    }
}

}  // namespace

void Start(bool verbose, const std::wstring& triggerPath) {
    if (!verbose) return;
    g_verbose.store(true, std::memory_order_relaxed);
    g_triggerPath = triggerPath;
    // A trigger left behind by the previous session would fire immediately.
    if (!g_triggerPath.empty()) DeleteFileW(g_triggerPath.c_str());
    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_stopEvent) {
        Log::Line("WARN: diagnostics could not create its stop event; verbose tracing is off");
        g_verbose.store(false, std::memory_order_relaxed);
        return;
    }
    g_running.store(true, std::memory_order_release);
    g_thread = std::thread(&Worker);
    Log::Line("Verbose diagnostics on: a trace every %d ms, and a screenshot whenever %ls appears",
              kTraceIntervalMs, g_triggerPath.c_str());
}

void Stop() {
    if (!g_running.exchange(false, std::memory_order_acq_rel)) return;
    if (g_stopEvent) SetEvent(g_stopEvent);
    if (g_thread.joinable()) g_thread.join();
    if (g_stopEvent) {
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
    }
}

bool IsVerbose() { return g_verbose.load(std::memory_order_relaxed); }

void Publish(const FrameTrace& trace) {
    if (!g_verbose.load(std::memory_order_relaxed)) return;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_latest = trace;
    g_haveFrame = true;
}

}  // namespace DyingLightHeadTracking::diagnostics
