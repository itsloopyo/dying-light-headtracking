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
constexpr int kFindPolls = 240;  // 60s, which covers a cold start off a hard disk.

// The window is still being sized for a moment after it appears, so the trigger
// is three seconds of an unchanged rect rather than a fixed delay. The budget for
// that is measured from the first sighting and kept short: a window still moving
// eight seconds later is being dragged, and centring it then undoes the drag.
constexpr int kSettlePolls = 12;
constexpr int kSettleBudgetPolls = 32;
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

int CenteredOrigin(LONG areaStart, LONG areaExtent, LONG windowExtent) {
    return static_cast<int>(areaStart + (areaExtent - windowExtent) / 2);
}

bool IsCenteredOn(const RECT& window, const RECT& area) {
    // Integer halving rounds differently from whatever centred the window, so an
    // exact comparison would move it one pixel and report that as a fix.
    constexpr int kTolerance = 2;
    const int dx = window.left - CenteredOrigin(area.left, area.right - area.left,
                                                window.right - window.left);
    const int dy = window.top - CenteredOrigin(area.top, area.bottom - area.top,
                                               window.bottom - window.top);
    return std::abs(dx) <= kTolerance && std::abs(dy) <= kTolerance;
}

void CenterUnlessPlaced(HWND window, const RECT& rect) {
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &info)) {
        Log::Line("WARN: window: GetMonitorInfoW failed: %lu", GetLastError());
        return;
    }

    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const RECT& work = info.rcWork;
    const int workWidth = work.right - work.left;
    const int workHeight = work.bottom - work.top;

    if (width >= workWidth || height >= workHeight) {
        Log::Line("window: %dx%d fills the %dx%d work area (fullscreen or borderless), leaving "
                  "it in place", width, height, workWidth, workHeight);
        return;
    }
    if (IsCenteredOn(rect, work) || IsCenteredOn(rect, info.rcMonitor)) {
        Log::Line("window: %dx%d at (%d, %d) is already centred, leaving it alone", width, height,
                  static_cast<int>(rect.left), static_cast<int>(rect.top));
        return;
    }
    // The engine creates the window at the monitor origin plus video.scr's
    // WindowOffset, which is 0,0 unless someone set it. Anywhere else is a
    // placement somebody chose.
    if (rect.left != info.rcMonitor.left || rect.top != info.rcMonitor.top) {
        Log::Line("window: %dx%d at (%d, %d) is not at the engine's default placement "
                  "(%d, %d), so it was placed on purpose; leaving it alone",
                  width, height, static_cast<int>(rect.left), static_cast<int>(rect.top),
                  static_cast<int>(info.rcMonitor.left), static_cast<int>(info.rcMonitor.top));
        return;
    }

    // The work area, not the monitor: centring on the monitor can put the title
    // bar behind a top-docked taskbar.
    const int x = CenteredOrigin(work.left, workWidth, width);
    const int y = CenteredOrigin(work.top, workHeight, height);

    // SWP_ASYNCWINDOWPOS because the window belongs to the game's thread, and a
    // synchronous cross-thread SetWindowPos blocks with no timeout until that
    // thread pumps, which it does not do during a level load.
    if (!SetWindowPos(window, nullptr, x, y, 0, 0,
                      SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS)) {
        Log::Line("WARN: window: SetWindowPos failed: %lu", GetLastError());
        return;
    }
    Log::Line("window: asked Windows to centre the %dx%d window at (%d, %d) on the %dx%d work "
              "area", width, height, x, y, workWidth, workHeight);
}

bool Poll() { return WaitForSingleObject(g_shutdownEvent, kPollMs) == WAIT_TIMEOUT; }

unsigned __stdcall CenterWhenSettled(void*) {
    HWND window = nullptr;
    RECT previous{};
    int stable = 0;
    int sinceSighting = 0;

    for (int poll = 0; poll < kFindPolls; ++poll) {
        if (!Poll()) return 0;

        const HWND current = FindGameWindow();
        RECT rect{};
        if (!current || !GetWindowRect(current, &rect)) {
            window = nullptr;
            continue;
        }
        if (current != window) {
            window = current;
            previous = rect;
            stable = 0;
            sinceSighting = 0;
            continue;
        }
        if (++sinceSighting > kSettleBudgetPolls) {
            Log::Line("window: the game window did not hold still within %lus of appearing, "
                      "leaving its placement alone",
                      static_cast<unsigned long>(kSettleBudgetPolls * kPollMs / 1000));
            return 0;
        }
        if (!EqualRect(&previous, &rect)) {
            previous = rect;
            stable = 0;
            continue;
        }
        if (++stable >= kSettlePolls) {
            CenterUnlessPlaced(window, rect);
            return 0;
        }
    }
    Log::Line("window: no settled game window within %lus, leaving placement alone",
              static_cast<unsigned long>(kFindPolls * kPollMs / 1000));
    return 0;
}

}  // namespace

void Start(HANDLE shutdownEvent) {
    g_shutdownEvent = shutdownEvent;
    g_thread = reinterpret_cast<HANDLE>(
        _beginthreadex(nullptr, 0, CenterWhenSettled, nullptr, 0, nullptr));
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
