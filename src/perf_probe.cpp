#include "perf_probe.h"

#include "cameraunlock/time/qpc_clock.h"

#include <cstdio>

namespace DyingLightHeadTracking::perf_probe {

Counter g_counters[kSiteCount];

namespace {

constexpr const char* kSiteNames[kSiteCount] = {"camera", "raytrace", "torch dir", "torch pos",
                                                "crosshair"};

// Written only by the camera hook's thread; read and reset by the heartbeat.
std::uint64_t g_lastFrameMicros = 0;
std::atomic<std::uint64_t> g_frames{0};
std::atomic<std::uint64_t> g_worstFrameMicros{0};

// The TSC rate is taken from the window itself against QPC, so there is no
// calibration sleep and no assumption about the CPU's nominal clock.
std::uint64_t g_windowTsc = 0;
std::uint64_t g_windowMicros = 0;
std::uint64_t g_lastCalls[kSiteCount] = {};
std::uint64_t g_lastTicks[kSiteCount] = {};

}  // namespace

void NoteFrame(std::uint64_t nowMicros) {
    if (g_lastFrameMicros) {
        const std::uint64_t frame = nowMicros - g_lastFrameMicros;
        if (frame > g_worstFrameMicros.load(std::memory_order_relaxed)) {
            g_worstFrameMicros.store(frame, std::memory_order_relaxed);
        }
    }
    g_lastFrameMicros = nowMicros;
    g_frames.fetch_add(1, std::memory_order_relaxed);
}

std::string TakeWindowReport() {
    const std::uint64_t tsc = __rdtsc();
    const std::uint64_t micros = cameraunlock::time::QpcNowMicros();
    const std::uint64_t frames = g_frames.exchange(0, std::memory_order_relaxed);
    const std::uint64_t worst = g_worstFrameMicros.exchange(0, std::memory_order_relaxed);

    std::uint64_t calls[kSiteCount], ticks[kSiteCount];
    for (int i = 0; i < kSiteCount; ++i) {
        const std::uint64_t c = g_counters[i].calls.load(std::memory_order_relaxed);
        const std::uint64_t t = g_counters[i].ticks.load(std::memory_order_relaxed);
        calls[i] = c - g_lastCalls[i];
        ticks[i] = t - g_lastTicks[i];
        g_lastCalls[i] = c;
        g_lastTicks[i] = t;
    }

    const bool firstWindow = g_windowMicros == 0;
    const double seconds = static_cast<double>(micros - g_windowMicros) * 1e-6;
    const double tscPerMs = static_cast<double>(tsc - g_windowTsc) / (seconds * 1000.0);
    g_windowTsc = tsc;
    g_windowMicros = micros;
    if (firstWindow || frames == 0 || !(seconds > 0.0)) return {};

    const double perFrame = 1.0 / static_cast<double>(frames);
    double totalMs = 0.0;
    char sites[512];
    int used = 0;
    for (int i = 0; i < kSiteCount; ++i) {
        const double ms = static_cast<double>(ticks[i]) / tscPerMs * perFrame;
        totalMs += ms;
        used += std::snprintf(sites + used, sizeof(sites) - used, "%s%s %.3f ms (%.0f calls)",
                              i ? ", " : "", kSiteNames[i], ms,
                              static_cast<double>(calls[i]) * perFrame);
    }

    char line[768];
    std::snprintf(line, sizeof(line),
                  "Perf: %.1f fps, worst frame %.1f ms | mod per frame %.3f ms: %s",
                  static_cast<double>(frames) / seconds, static_cast<double>(worst) * 1e-3,
                  totalMs, sites);
    return line;
}

}  // namespace DyingLightHeadTracking::perf_probe
