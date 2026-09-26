#pragma once

#include "config.h"

#include "cameraunlock/protocol/udp_receiver.h"
#include "cameraunlock/time/frame_clock.h"
#include "cameraunlock/tracking/head_tracking_session.h"
#include "cameraunlock/tracking/tracking_mode.h"

#include <atomic>

namespace DyingLightHeadTracking {

// One frame's processed head pose, as the core pipeline hands it over: rotation
// in degrees and position in metres, in the tracker's own convention. The
// conversion to the engine happens once, in view_pose.h.
struct FrameSample {
    bool has_rotation = false;
    float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
    bool has_position = false;
    float pos_x = 0.0f, pos_y = 0.0f, pos_z = 0.0f;
};

class TrackingRuntime {
public:
    TrackingRuntime() : m_session(m_receiver) {}

    // No return value: a receiver that cannot bind immediately is not a failure.
    // The port is usually held by another mod still shutting down, and core's
    // supervisor retries until it frees, so tracking starts when the port does.
    void Start(const Config& cfg);
    void Stop();

    // Runs the per-frame pipeline once and returns the processed pose. Called
    // from the camera hook, on the game's own thread.
    FrameSample SampleFrame();

    bool IsReceiving() const { return m_receiver.IsReceiving(); }

    void ToggleEnabled();
    // Each returns the state it switched to.
    cameraunlock::TrackingMode CycleTrackingMode();
    bool ToggleYawMode();

    bool IsEnabled() const { return m_enabled.load(std::memory_order_relaxed); }
    bool IsWorldSpaceYaw() const { return m_worldSpaceYaw.load(std::memory_order_relaxed); }

private:
    static constexpr float kMaxFrameDtSec = 0.25f;

    // True while the newest packet is younger than Config::data_freshness_ms.
    bool IsPoseFresh() const;

    // m_held masked by the tracking mode in force right now.
    FrameSample HeldForCurrentMode() const;

    Config m_cfg{};
    cameraunlock::UdpReceiver m_receiver;
    cameraunlock::HeadTrackingSession<cameraunlock::UdpReceiver> m_session;
    cameraunlock::time::FrameClock m_clock{kMaxFrameDtSec};

    // The pose last injected, re-applied while the tracker is quiet so a gap
    // holds the view where it was. Touched only from SampleFrame.
    FrameSample m_held{};

    std::atomic<bool> m_started{false};
    std::atomic<bool> m_enabled{false};
    std::atomic<bool> m_worldSpaceYaw{false};
};

}  // namespace DyingLightHeadTracking
