#include "world_query.h"

#include "engine_api.h"
#include "logging.h"
#include "perf_probe.h"

#include <MinHook.h>
#include <windows.h>

#include <atomic>
#include <cmath>
#include <cstring>

namespace DyingLightHeadTracking::world_query {

namespace {

// unsigned char IGSObject::Raytrace(const IRayCache* cache, unsigned char mode,
//                                   SCollision* out, const vec3& start, vec3& end,
//                                   unsigned short f, IControlObject* ignore,
//                                   unsigned int g, unsigned short h, int i)
//
// The overload without a cache calls this one with a null cache, and the game
// calls both, so this is the one that sees every trace. The mod replays with a
// null cache, as the plain overload does.
//
// `end` is in/out: the engine writes the stop point into it. The mode byte is
// 0x37 at the overwhelming majority of the game's own call sites, but the mod
// does not hardcode it - the captured call supplies whatever the game used.
using RaytraceFn = unsigned char (*)(void* thiz, const void* cache, unsigned char mode,
                                     void* collision, const engine::Vec3* start,
                                     engine::Vec3* end, unsigned short f, void* ignore,
                                     unsigned int g, unsigned int h, int i);

// The engine copies 0x3C bytes of result into SCollision; this is a generous
// over-allocation, zeroed before every call, and 16-byte aligned because the
// engine writes vectors into it.
constexpr std::size_t kCollisionBytes = 1024;

struct CapturedContext {
    void* thiz = nullptr;
    unsigned char mode = 0;
    unsigned short f = 0;
    void* ignore = nullptr;
    unsigned int g = 0;
    unsigned int h = 0;
    int i = 0;
    ULONGLONG tick = 0;
};

void* g_target = nullptr;
RaytraceFn g_original = nullptr;

// The context is re-taken from every qualifying game call rather than once per
// level. The object pointers in it are live game objects, and a copy held for
// minutes is one the game may have freed since; replaying through a freed object
// reads whatever was allocated there next.
SRWLOCK g_lock = SRWLOCK_INIT;
CapturedContext g_context;
bool g_haveContext = false;

// A context the game has not refreshed within this long is not replayed.
constexpr ULONGLONG kMaxContextAgeMs = 250;

// The camera the hook last saw. Written from the camera hook, read inside the
// detour, which runs on other threads too.
std::atomic<float> g_camX{0.0f}, g_camY{0.0f}, g_camZ{0.0f};
std::atomic<float> g_fwdX{0.0f}, g_fwdY{0.0f}, g_fwdZ{0.0f};
std::atomic<bool> g_camKnown{false};

// Set while the mod is inside its own Cast, so the detour does not treat the
// mod's own trace as a candidate to capture.
thread_local bool t_inOwnCast = false;

// How close a trace's origin must be to the camera to count as the shot's own.
// The weapon trace starts at the eye; a metre of slack covers the muzzle offset
// some weapons use without admitting traces that start at a zombie's foot.
constexpr float kCameraOriginTolerance = 1.0f;

// And how closely it must run along the clean camera's forward axis. The game
// also casts from the eye in other directions, with other filters; only the one
// along the view is the aim trace. It follows the clean camera, not the
// head-tracked one: aim is decoupled.
constexpr float kMinForwardAlignment = 0.999f;

Vec3f CleanForward() {
    return {g_fwdX.load(std::memory_order_relaxed), g_fwdY.load(std::memory_order_relaxed),
            g_fwdZ.load(std::memory_order_relaxed)};
}

bool StartsAtCamera(const engine::Vec3* start) {
    const float dx = start->x - g_camX.load(std::memory_order_relaxed);
    const float dy = start->y - g_camY.load(std::memory_order_relaxed);
    const float dz = start->z - g_camZ.load(std::memory_order_relaxed);
    return dx * dx + dy * dy + dz * dz < kCameraOriginTolerance * kCameraOriginTolerance;
}

bool AlongCleanForward(const Vec3f& ray, float len) {
    return Dot(ray, CleanForward()) / len >= kMinForwardAlignment;
}

// What the game's own aim trace found, straight out of its call. The game casts
// more than one ray along the view each frame (a short one with an interaction
// filter and a 25 m one with the weapon filter); the longest one seen recently
// is the one kept, so the depth does not flip between the two filters.
struct AimResult {
    bool have = false;
    bool hit = false;
    Vec3f start;
    Vec3f point;
    float length = 0.0f;
    ULONGLONG tick = 0;
};
AimResult g_aim;
bool g_loggedAimFilter = false;

bool RecordAimResult(const engine::Vec3* start, const engine::Vec3* end, float length,
                     unsigned char result, unsigned char mode, unsigned int g, unsigned int h,
                     int i) {
    const ULONGLONG now = GetTickCount64();
    AcquireSRWLockExclusive(&g_lock);
    const bool fresh = g_aim.have && now - g_aim.tick <= kMaxContextAgeMs;
    const bool take = !fresh || length >= g_aim.length * 0.99f;
    const bool logFilter = take && !g_loggedAimFilter;
    if (take) {
        g_aim.have = true;
        g_aim.hit = result != 0;
        g_aim.start = {start->x, start->y, start->z};
        g_aim.point = {end->x, end->y, end->z};
        g_aim.length = length;
        g_aim.tick = now;
        if (logFilter) g_loggedAimFilter = true;
    }
    ReleaseSRWLockExclusive(&g_lock);
    if (logFilter) {
        Log::Line("World query: reading the depth off the game's own aim trace (%.1f m, "
                  "mode=0x%02X g=0x%X h=0x%X i=%d)", static_cast<double>(length), mode, g, h, i);
    }
    return take;
}

void Capture(const CapturedContext& c) {
    AcquireSRWLockExclusive(&g_lock);
    g_context = c;
    g_haveContext = true;
    ReleaseSRWLockExclusive(&g_lock);
}

unsigned char Detour(void* thiz, const void* cache, unsigned char mode, void* collision,
                     const engine::Vec3* start, engine::Vec3* end, unsigned short f, void* ignore,
                     unsigned int g, unsigned int h, int i) {
    perf_probe::Scope perf(perf_probe::kRaytrace);
    float len = 0.0f;
    bool aimTrace = false;
    if (!t_inOwnCast && thiz && start && end && g_camKnown.load(std::memory_order_relaxed) &&
        StartsAtCamera(start)) {
        const Vec3f ray{end->x - start->x, end->y - start->y, end->z - start->z};
        len = Length(ray);
        aimTrace = len > 1e-3f && AlongCleanForward(ray, len);
    }
    perf.Pause();
    const unsigned char result =
        g_original(thiz, cache, mode, collision, start, end, f, ignore, g, h, i);
    perf.Resume();
    // Only the trace whose result is kept supplies the replay context, so the
    // lean clamp's replay and the reticle's depth use one filter.
    if (aimTrace && RecordAimResult(start, end, len, result, mode, g, h, i)) {
        CapturedContext c;
        c.thiz = thiz;
        c.mode = mode;
        c.f = f;
        c.ignore = ignore;
        c.g = g;
        c.h = h;
        c.i = i;
        c.tick = GetTickCount64();
        Capture(c);
    }
    return result;
}

bool g_loggedStale = false;

}  // namespace

bool Install() {
    g_target = engine::RaytraceTarget();
    if (!g_target) return false;

    MH_STATUS status = MH_CreateHook(g_target, reinterpret_cast<LPVOID>(&Detour),
                                     reinterpret_cast<LPVOID*>(&g_original));
    if (status == MH_OK) status = MH_EnableHook(g_target);
    if (status != MH_OK) {
        Log::Line("ERROR: hooking IGSObject::Raytrace @ %p failed: %s", g_target,
                  MH_StatusToString(status));
        MH_RemoveHook(g_target);
        g_target = nullptr;
        return false;
    }
    Log::Line("World query watching IGSObject::Raytrace @ %p", g_target);
    return true;
}

void Remove() {
    if (!g_target) return;
    MH_DisableHook(g_target);
    MH_RemoveHook(g_target);
    g_target = nullptr;
    ForgetContext();
}

void ForgetContext() {
    AcquireSRWLockExclusive(&g_lock);
    g_haveContext = false;
    ReleaseSRWLockExclusive(&g_lock);
}

void NoteCamera(const Vec3f& pos, const Vec3f& forward) {
    g_camX.store(pos.x, std::memory_order_relaxed);
    g_camY.store(pos.y, std::memory_order_relaxed);
    g_camZ.store(pos.z, std::memory_order_relaxed);
    g_fwdX.store(forward.x, std::memory_order_relaxed);
    g_fwdY.store(forward.y, std::memory_order_relaxed);
    g_fwdZ.store(forward.z, std::memory_order_relaxed);
    g_camKnown.store(true, std::memory_order_relaxed);
}

TraceHit GameAimHit() {
    TraceHit out;
    AcquireSRWLockShared(&g_lock);
    const AimResult aim = g_aim;
    ReleaseSRWLockShared(&g_lock);
    if (!aim.have || GetTickCount64() - aim.tick > kMaxContextAgeMs) return out;
    out.queried = true;
    if (!aim.hit) return out;
    const float distance = Length(Sub(aim.point, aim.start));
    if (!std::isfinite(distance)) return out;
    out.blocked = true;
    out.distance = distance;
    out.point = aim.point;
    return out;
}


TraceHit Cast(const Vec3f& start, const Vec3f& direction, float maxDistance) {
    TraceHit out;
    if (!g_original || !(maxDistance > 0.0f)) return out;

    AcquireSRWLockShared(&g_lock);
    const bool have = g_haveContext;
    const CapturedContext ctx = g_context;
    ReleaseSRWLockShared(&g_lock);
    if (!have) return out;

    // Only replayed while the game is still making the trace, so the objects in
    // the context are ones the game itself called through moments ago.
    if (GetTickCount64() - ctx.tick > kMaxContextAgeMs) {
        if (!g_loggedStale) {
            g_loggedStale = true;
            Log::Line("World query: the game has gone more than %llums without its aim trace, so "
                      "the last context is not replayed (logged once)", kMaxContextAgeMs);
        }
        return out;
    }

    alignas(16) static thread_local unsigned char collision[kCollisionBytes];
    std::memset(collision, 0, sizeof(collision));

    const engine::Vec3 from{start.x, start.y, start.z};
    engine::Vec3 to{start.x + direction.x * maxDistance, start.y + direction.y * maxDistance,
                    start.z + direction.z * maxDistance};

    t_inOwnCast = true;
    const unsigned char result = g_original(ctx.thiz, nullptr, ctx.mode, collision, &from, &to,
                                            ctx.f, ctx.ignore, ctx.g, ctx.h, ctx.i);
    t_inOwnCast = false;

    out.queried = true;
    if (result == 0) return out;

    const Vec3f hit{to.x, to.y, to.z};
    const Vec3f travel = Sub(hit, start);
    const float distance = Length(travel);
    if (!std::isfinite(distance) || distance > maxDistance * 1.01f) return out;

    out.blocked = true;
    out.distance = distance;
    out.point = hit;
    return out;
}

}  // namespace DyingLightHeadTracking::world_query
