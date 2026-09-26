#pragma once

#include "config.h"

#include "cameraunlock/input/hotkey_poller.h"

#include <atomic>
#include <functional>

namespace DyingLightHeadTracking {

class Hotkeys {
public:
    using Action = std::function<void()>;

    bool Start(const Config& cfg, Action onToggle, Action onCycleMode,
               Action onYawMode, Action onReticle);
    void Stop();

private:
    cameraunlock::input::HotkeyPoller m_poller;
    // Both the init thread and DLL_PROCESS_DETACH can call Stop(); claiming this
    // flag leaves exactly one of them to join the poller thread.
    std::atomic<bool> m_started{false};
};

}  // namespace DyingLightHeadTracking
