#pragma once

#include <cstddef>
#include <cstring>

namespace DyingLightHeadTracking {

// Why head tracking is or is not being applied this frame. One value, so the log
// and the heartbeat can name the reason rather than reporting a bare "off".
enum class GateReason {
    Ready,
    NoLevel,
    Loading,
    Warmup,
    TimerFrozen,     // paused, in a menu, or in a non-interactive sequence
    InputsDisabled,  // cutscene, dialogue, or a UI screen that owns the input
    NoActiveView,
    MainMenu,
    Multiplayer,
    NotCalibrated,
};

inline const char* GateReasonText(GateReason reason) {
    switch (reason) {
        case GateReason::Ready: return "gameplay";
        case GateReason::NoLevel: return "no level";
        case GateReason::Loading: return "loading";
        case GateReason::Warmup: return "post-load warmup";
        case GateReason::TimerFrozen: return "paused or in a menu";
        case GateReason::InputsDisabled: return "inputs disabled (cutscene, dialogue or UI)";
        case GateReason::NoActiveView: return "no active view";
        case GateReason::MainMenu: return "front end";
        case GateReason::Multiplayer: return "multiplayer session";
        case GateReason::NotCalibrated: return "screen convention not measured yet";
    }
    return "unknown";
}

namespace detail {

// Case-insensitive substring, for a level path whose casing is not ours to
// assume.
inline bool ContainsFold(const char* haystack, const char* needle) {
    if (!haystack || !needle) return false;
    const std::size_t n = std::strlen(needle);
    for (const char* p = haystack; *p; ++p) {
        std::size_t i = 0;
        while (i < n && p[i] && (p[i] | 0x20) == (needle[i] | 0x20)) {
            ++i;
        }
        if (i == n) return true;
    }
    return false;
}

}  // namespace detail

// Levels whose name marks them as the front end rather than a playable map.
// Dying Light's front end is its own level. The name is logged whenever it
// changes so this list can be checked against what the game actually loads
// rather than against a guess.
inline bool IsFrontEndLevel(const char* levelName) {
    return detail::ContainsFold(levelName, "menu") || detail::ContainsFold(levelName, "frontend");
}

}  // namespace DyingLightHeadTracking
