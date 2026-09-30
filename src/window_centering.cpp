#include "window_centering.h"

#include "logging.h"

#include <process.h>

#include <cstdlib>
#include <cwchar>

namespace DyingLightHeadTracking::window_centering {

namespace {

// The splash the engine shows first is a separate, already centred pop-up, and
// the chooser and the game proper both render into this one window.
constexpr const wchar_t* kGameWindowClass = L"techland_game_class";

constexpr DWORD kPollMs = 250;

// The engine sizes the window in several steps as it appears and again after a
// resolution change, so a rect counts only once it has held still this long.
constexpr int kSettlePolls = 8;

// The engine creates the window at the monitor origin plus video.scr's
// WindowOffset, which is 0,0 unless someone set it. The frame's invisible
// resize border can sit a few pixels outside the monitor edge, so the origin
// test allows this much.
constexpr LONG kOriginTolerance = 16;

// Integer halving rounds differently from whatever centred the window, so an
// exact comparison would move it one pixel and report that as a fix.
constexpr LONG kCentredTolerance = 2;

constexpr DWORD kThreadJoinMs = 2000;

HANDLE g_thread = nullptr;
HANDLE g_shutdownEvent = nullptr;

struct FindState {
    DWORD pid;
    HWND found;
};

BOOL CALLBACK MatchGameWindow(HWND hwnd, LPARAM param) {
    auto* state = reinterpret_cast<FindState*>(param);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != state->pid || !IsWindowVisible(hwnd)) return TRUE;
    wchar_t cls[64];
    if (GetClassNameW(hwnd, cls, 64) == 0 || std::wcscmp(cls, kGameWindowClass) != 0) return TRUE;
    state->found = hwnd;
    return FALSE;
}

HWND FindGameWindow() {
    FindState state{GetCurrentProcessId(), nullptr};
    EnumWindows(MatchGameWindow, reinterpret_cast<LPARAM>(&state));
    return state.found;
}

// Fullscreen and borderless both run in a caption-less pop-up (style 0x14000000
// at 2560x1440 on this machine); windowed mode has a title bar. The size says
// nothing either way: a 2560x1440 client in its frame is taller than the work
// area of a 5120x1440 monitor and is still a window.
bool IsWindowed(HWND window) {
    return (GetWindowLongPtrW(window, GWL_STYLE) & WS_CAPTION) == WS_CAPTION;
}

// Centred on the work area along an axis the window fits in, so the title bar
// never ends up behind a docked taskbar, and on the whole monitor along an axis
// it does not, so what spills over spills evenly.
LONG CentredOrigin(LONG workStart, LONG workExtent, LONG monStart, LONG monExtent, LONG size) {
    return size <= workExtent ? workStart + (workExtent - size) / 2
                              : monStart + (monExtent - size) / 2;
}

bool Near(LONG a, LONG b, LONG tolerance) { return std::abs(a - b) <= tolerance; }

class Placer {
public:
    // Called once per settled rect the mod has not acted on yet.
    void OnSettled(HWND window, const RECT& rect) {
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        if (!GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &info)) {
            Log::Line("WARN: window: GetMonitorInfoW failed: %lu", GetLastError());
            return;
        }
        const RECT& mon = info.rcMonitor;
        const RECT& work = info.rcWork;
        const LONG width = rect.right - rect.left;
        const LONG height = rect.bottom - rect.top;

        const bool windowed = IsWindowed(window);
        const bool wasWindowed = m_haveHandled && m_handledWindowed;
        const bool first = !m_haveHandled;
        const RECT previous = m_handled;
        Remember(rect, windowed);

        if (!windowed) {
            if (first || wasWindowed) {
                Log::Line("window: %ldx%ld fullscreen or borderless, leaving it in place", width,
                          height);
            }
            return;
        }
        if (m_playerPlaced) return;

        // Where the engine is the one that put the window here, it goes in the
        // centre. Anything else was the player, and stays where they put it.
        const char* why = nullptr;
        if (first) {
            if (!Near(rect.left, mon.left, kOriginTolerance) ||
                !Near(rect.top, mon.top, kOriginTolerance)) {
                LeaveToPlayer(rect, "opened away from the engine's default placement "
                                    "(video.scr WindowOffset)");
                return;
            }
            why = "opened at the monitor origin";
        } else if (!wasWindowed) {
            why = "switched from fullscreen";
        } else if (rect.left == previous.left && rect.top == previous.top) {
            // The renderer's only SetWindowPos passes SWP_NOMOVE, so a resize
            // made by the engine keeps the top-left corner where it was.
            why = "resized by the game in place";
        } else {
            LeaveToPlayer(rect, "moved by the player");
            return;
        }

        const LONG x = CentredOrigin(work.left, work.right - work.left, mon.left,
                                     mon.right - mon.left, width);
        const LONG y = CentredOrigin(work.top, work.bottom - work.top, mon.top,
                                     mon.bottom - mon.top, height);
        if (Near(rect.left, x, kCentredTolerance) && Near(rect.top, y, kCentredTolerance)) return;

        // SWP_ASYNCWINDOWPOS because the window belongs to the game's thread, and a
        // synchronous cross-thread SetWindowPos blocks with no timeout until that
        // thread pumps, which it does not do during a level load.
        if (!SetWindowPos(window, nullptr, x, y, 0, 0,
                          SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS)) {
            Log::Line("WARN: window: SetWindowPos failed: %lu", GetLastError());
            return;
        }
        // The move lands as a new rect; remembering it here keeps the mod's own
        // move from reading as the player's.
        const RECT target{x, y, x + width, y + height};
        Remember(target, true);
        Log::Line("window: %ldx%ld %s at (%ld, %ld), centring it at (%ld, %ld) on the %ldx%ld "
                  "work area of a %ldx%ld monitor",
                  width, height, why, rect.left, rect.top, x, y, work.right - work.left,
                  work.bottom - work.top, mon.right - mon.left, mon.bottom - mon.top);
    }

    bool AlreadyHandled(const RECT& rect) const {
        return m_haveHandled && EqualRect(&rect, &m_handled);
    }

private:
    void Remember(const RECT& rect, bool windowed) {
        m_handled = rect;
        m_handledWindowed = windowed;
        m_haveHandled = true;
    }

    void LeaveToPlayer(const RECT& rect, const char* reason) {
        m_playerPlaced = true;
        Log::Line("window: %ldx%ld at (%ld, %ld) was %s; leaving its placement alone for the rest "
                  "of the session",
                  rect.right - rect.left, rect.bottom - rect.top, rect.left, rect.top, reason);
    }

    RECT m_handled{};
    bool m_handledWindowed = false;
    bool m_haveHandled = false;
    bool m_playerPlaced = false;
};

bool Poll() { return WaitForSingleObject(g_shutdownEvent, kPollMs) == WAIT_TIMEOUT; }

// Runs for the whole session: the engine resizes the window when the player
// picks a game on the chooser, changes resolution or leaves fullscreen, and each
// of those leaves it where its top-left corner was.
unsigned __stdcall WatchWindow(void*) {
    HWND window = nullptr;
    RECT seen{};
    int stable = 0;
    Placer placer;

    while (Poll()) {
        if (!window || !IsWindow(window)) {
            window = FindGameWindow();
            placer = Placer{};
            stable = 0;
            if (window && !GetWindowRect(window, &seen)) window = nullptr;
            continue;
        }
        RECT rect{};
        if (!GetWindowRect(window, &rect)) {
            window = nullptr;
            continue;
        }
        if (!EqualRect(&rect, &seen)) {
            seen = rect;
            stable = 0;
            continue;
        }
        if (stable < kSettlePolls) ++stable;
        if (stable < kSettlePolls || placer.AlreadyHandled(rect)) continue;
        // A button held down is the player mid-drag on the frame; the rect it
        // settles on once released is theirs.
        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) continue;
        placer.OnSettled(window, rect);
    }
    return 0;
}

}  // namespace

void Start(HANDLE shutdownEvent) {
    g_shutdownEvent = shutdownEvent;
    g_thread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, WatchWindow, nullptr, 0, nullptr));
    if (!g_thread) {
        Log::Line("WARN: window: could not start the centring thread; the window stays where "
                  "the game puts it");
    }
}

void Stop() {
    if (!g_thread) return;
    if (WaitForSingleObject(g_thread, kThreadJoinMs) != WAIT_OBJECT_0) {
        Log::Line("WARN: window: the centring thread did not exit within 2s");
    }
    CloseHandle(g_thread);
    g_thread = nullptr;
}

}  // namespace DyingLightHeadTracking::window_centering
