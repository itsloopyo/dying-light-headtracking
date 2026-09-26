#include "hotkeys.h"

#include "logging.h"

#include "cameraunlock/input/chord_hotkeys.h"

#include <exception>

namespace DyingLightHeadTracking {

namespace {

constexpr int kPollIntervalMs = 16;

}  // namespace

bool Hotkeys::Start(const Config& cfg, Action onToggle, Action onCycleMode,
                    Action onYawMode, Action onReticle) {
    if (m_started.load(std::memory_order_acquire)) return true;

    using cameraunlock::input::ChordGuarded;
    using cameraunlock::input::NavGuarded;

    // Nav-cluster keys are ignored while Ctrl+Shift is held, so a chord is the
    // only trigger for Ctrl+Shift+<nav> and one press never fires twice.
    m_poller.SetToggleKey(cfg.vk_toggle, NavGuarded(onToggle));
    m_poller.AddHotkey(cfg.vk_cycle_mode, NavGuarded(onCycleMode));
    m_poller.AddHotkey(cfg.vk_yaw_mode, NavGuarded(onYawMode));
    m_poller.AddHotkey(cfg.vk_reticle, NavGuarded(onReticle));

    if (cfg.chord_toggle)     m_poller.AddHotkey('Y', ChordGuarded(std::move(onToggle)));
    if (cfg.chord_cycle_mode) m_poller.AddHotkey('G', ChordGuarded(std::move(onCycleMode)));
    if (cfg.chord_yaw_mode)   m_poller.AddHotkey('H', ChordGuarded(std::move(onYawMode)));
    if (cfg.chord_reticle)    m_poller.AddHotkey('U', ChordGuarded(std::move(onReticle)));

    // Reached from a __stdcall thread procedure, where an escaping exception is
    // std::terminate and the game would vanish with the reason unwritten.
    try {
        if (!m_poller.Start(kPollIntervalMs)) {
            Log::Line("ERROR: HotkeyPoller failed to start");
            return false;
        }
    } catch (const std::exception& e) {
        Log::Line("ERROR: HotkeyPoller failed to start: %s", e.what());
        return false;
    }

    Log::Line("Hotkeys: toggle=0x%02X cyclemode=0x%02X yawmode=0x%02X reticle=0x%02X",
              cfg.vk_toggle, cfg.vk_cycle_mode, cfg.vk_yaw_mode, cfg.vk_reticle);

    m_started.store(true, std::memory_order_release);
    return true;
}

void Hotkeys::Stop() {
    if (m_started.exchange(false, std::memory_order_acq_rel)) {
        m_poller.Stop();
    }
}

}  // namespace DyingLightHeadTracking
