#include "tracking_runtime.h"

#include "logging.h"

#include <chrono>
#include <cstdint>

namespace DyingLightHeadTracking {

void TrackingRuntime::Start(const Config& cfg) {
    m_cfg = cfg;

    // No sensitivity, deadzone or inversion is set: the core defaults are 1:1
    // with neither, which is the pose the tracker sent. Shaping it is the tracker
    // app's job, so one profile behaves the same in every game. The limits and the
    // smoothing copies come from the config.
    m_session.SetPositionSettings(m_cfg.position);

    // One value feeds both the rotation and the position processor - there is no
    // separate position smoothing setting - picked per connection from the
    // receiver's IsRemoteConnection(), re-read on every Update().
    static_assert(decltype(m_session)::kHasRemoteConnection,
                  "receiver must expose IsRemoteConnection() or smoothing silently stays local");
    m_session.SetLocalSmoothing(m_cfg.local_smoothing);
    m_session.SetRemoteSmoothing(m_cfg.remote_smoothing);

    m_enabled.store(m_cfg.enable_on_startup, std::memory_order_relaxed);
    m_worldSpaceYaw.store(m_cfg.world_space_yaw, std::memory_order_relaxed);
    // The table never loads a pair that names no mode: it reads both as their defaults
    // instead.
    m_session.SetMode(
        cameraunlock::DecodeTrackingMode(m_cfg.rotation_enabled, m_cfg.position_enabled).value());

    m_receiver.SetLog([](const std::string& msg) { Log::Line("UDP: %s", msg.c_str()); });

    m_started.store(true, std::memory_order_release);

    const auto port = static_cast<std::uint16_t>(m_cfg.udp_port);
    if (m_receiver.Start(port)) {
        Log::Line("UDP receiver listening on port %u", port);
    } else {
        Log::Line("WARN: UDP receiver did not bind immediately on port %u; background retry active",
                  port);
    }
}

void TrackingRuntime::Stop() {
    // Both the init thread and DLL_PROCESS_DETACH can reach here, and the
    // receiver's own Stop() tests a flag before joining, so two callers can
    // arrive at the join together. Claiming the flag leaves exactly one to do it.
    if (m_started.exchange(false, std::memory_order_acq_rel)) {
        m_receiver.Stop();
    }
}

void TrackingRuntime::ToggleEnabled() {
    const bool prev = m_enabled.load(std::memory_order_relaxed);
    m_enabled.store(!prev, std::memory_order_relaxed);
    Log::Line("Tracking %s", !prev ? "enabled" : "disabled");
}

cameraunlock::TrackingMode TrackingRuntime::CycleTrackingMode() {
    const cameraunlock::TrackingMode mode = m_session.CycleMode();
    switch (mode) {
        case cameraunlock::TrackingMode::RotationAndPosition:
            Log::Line("Tracking mode: rotation + position (6DOF)");
            break;
        case cameraunlock::TrackingMode::RotationOnly:
            Log::Line("Tracking mode: rotation only");
            break;
        case cameraunlock::TrackingMode::PositionOnly:
            Log::Line("Tracking mode: position only");
            break;
    }
    return mode;
}

bool TrackingRuntime::ToggleYawMode() {
    const bool prev = m_worldSpaceYaw.load(std::memory_order_relaxed);
    m_worldSpaceYaw.store(!prev, std::memory_order_relaxed);
    Log::Line("Yaw mode: %s", !prev ? "world-space (horizon-locked)" : "camera-local");
    return !prev;
}

bool TrackingRuntime::IsPoseFresh() const {
    const std::int64_t lastUs = m_receiver.GetLastReceiveTimestamp();
    if (lastUs == 0) return false;
    const std::int64_t nowUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count();
    return (nowUs - lastUs) / 1000 < m_cfg.data_freshness_ms;
}

FrameSample TrackingRuntime::SampleFrame() {
    // Switched off is not tracking lost: the player asked for the game's own
    // camera back, so the held pose is dropped rather than re-applied on the next
    // enable.
    if (!m_enabled.load(std::memory_order_relaxed)) {
        m_held = FrameSample{};
        return m_held;
    }

    // Ticked unconditionally, including on a held frame. Skipping it while the
    // tracker is quiet makes the first fresh frame arrive with the whole gap as
    // its delta, and at the clamp the smoothing factor is near 1, so the view
    // snaps instead of blending - the snap the hold exists to prevent, moved to
    // the other end of the gap.
    const float dt = m_clock.Tick();

    if (!IsPoseFresh() || !m_session.Update(dt)) {
        return HeldForCurrentMode();
    }

    FrameSample out;
    out.has_rotation = m_session.GetRotation(out.yaw, out.pitch, out.roll);
    out.has_position = m_session.GetPositionOffset(out.pos_x, out.pos_y, out.pos_z);
    m_held = out;
    return out;
}

// The held pose predates whatever mode the player has since cycled to, and a
// held frame never reaches m_session.Update(), which is where the mode is
// normally applied. Without this, cycling to rotation-only while the tracker is
// quiet leaves the stale lean on screen until packets resume - and the player
// presses the key precisely because the lean is in the way.
FrameSample TrackingRuntime::HeldForCurrentMode() const {
    FrameSample out = m_held;
    if (!m_session.IsRotationActive()) {
        out.has_rotation = false;
        out.yaw = out.pitch = out.roll = 0.0f;
    }
    if (!m_session.IsPositionActive()) {
        out.has_position = false;
        out.pos_x = out.pos_y = out.pos_z = 0.0f;
    }
    return out;
}

}  // namespace DyingLightHeadTracking
