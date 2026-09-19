#pragma once

#include <intrin.h>

#include <atomic>
#include <cstdint>
#include <string>

// Frame rate and the time the mod's own code spends inside each hook, logged by
// the heartbeat. Time spent in the game's original function is excluded, so a
// hook that is called thousands of times a frame shows up as what it costs the
// game rather than as what the game was already spending there.
namespace DyingLightHeadTracking::perf_probe {

enum Site : int { kCamera, kRaytrace, kTorchDir, kTorchPos, kCrosshair, kSiteCount };

struct Counter {
    std::atomic<std::uint64_t> calls{0};
    std::atomic<std::uint64_t> ticks{0};
};

extern Counter g_counters[kSiteCount];

// Times the enclosing detour, less whatever runs between Pause() and Resume().
class Scope {
public:
    explicit Scope(Site site) : m_site(site), m_start(__rdtsc()) {}
    ~Scope() {
        Counter& c = g_counters[m_site];
        c.calls.fetch_add(1, std::memory_order_relaxed);
        c.ticks.fetch_add(m_spent + (m_paused ? 0 : __rdtsc() - m_start),
                          std::memory_order_relaxed);
    }
    void Pause() {
        m_spent += __rdtsc() - m_start;
        m_paused = true;
    }
    void Resume() {
        m_start = __rdtsc();
        m_paused = false;
    }

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

private:
    Site m_site;
    std::uint64_t m_start;
    std::uint64_t m_spent = 0;
    bool m_paused = false;
};

// One call per rendered frame, from the camera hook's first call of the frame.
void NoteFrame(std::uint64_t nowMicros);

// Everything since the previous call, as one log line; empty when no frame was
// seen in the window.
std::string TakeWindowReport();

}  // namespace DyingLightHeadTracking::perf_probe
