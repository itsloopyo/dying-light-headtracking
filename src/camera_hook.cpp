#include "camera_hook.h"

#include "aim_projection.h"
#include "diagnostics.h"
#include "engine_api.h"
#include "engine_convert.h"
#include "fov_reference.h"
#include "game_state.h"
#include "lean_trace.h"
#include "logging.h"
#include "flashlight.h"
#include "perf_probe.h"
#include "hud_crosshair.h"
#include "screen_calibration.h"
#include "view_pose.h"
#include "world_query.h"

#include "cameraunlock/camera/lean_clamp.h"
#include "cameraunlock/camera/zoom_compensation.h"
#include "cameraunlock/time/qpc_clock.h"

#include <MinHook.h>
#include <windows.h>

#include <atomic>
#include <cmath>

namespace DyingLightHeadTracking {

namespace {

using FromForwardUpPosFn = void (*)(engine::Camera*, const engine::Vec3*, const engine::Vec3*,
                                    const engine::Vec3*);

// The engine can drive the same camera more than once in a frame. Re-running the
// pipeline would advance smoothing twice against a near-zero delta; this reuses
// the frame's pose instead.
constexpr std::uint64_t kSameFrameMicros = 1000;

// The lean clamp's release ease runs on the time since the last frame that
// applied a lean. The first such frame, and any gap longer than the cap (a pause,
// a load), is treated as one nominal frame rather than as the whole gap.
constexpr float kNominalFrameDtSec = 0.016f;
constexpr float kMaxLeanDtSec = 0.25f;

constexpr float kRadToDeg = 57.2957795f;

// A posed frame with no aim trace to read is normal for the first frames of a
// level. This many in a row is the game having stopped making it, which is
// worth one log line.
constexpr int kMissingAimFramesToReport = 300;

void* g_target = nullptr;
FromForwardUpPosFn g_original = nullptr;

TrackingRuntime* g_tracking = nullptr;
Config g_cfg;

ScreenConvention g_convention;
bool g_calibrated = false;
const engine::Level* g_lastLevel = nullptr;

cameraunlock::camera::LeanClamp g_leanClamp;
lean_trace::Context g_leanContext;

std::atomic<unsigned long long> g_updates{0};
std::atomic<unsigned long long> g_posed{0};
std::atomic<int> g_gateReason{static_cast<int>(GateReason::NoLevel)};
std::atomic<bool> g_multiplayer{false};

// One pose per frame, cached across repeat calls for the same camera.
std::uint64_t g_lastSampleMicros = 0;
std::uint64_t g_lastAppliedMicros = 0;
std::uint64_t g_lastFrameMicros = 0;
FrameSample g_frameSample;

bool g_loggedBasis = false;
bool g_loggedTangents = false;
bool g_loggedZoomBasis = false;
bool g_loggedNoZoomReference = false;
int g_missingAimFrames = 0;
bool g_loggedNoQuery = false;
bool g_lastLeanContact = false;
bool g_lastLeanQueryFailed = false;

// Last frame's aim point, for the cross-check in CheckAgainstEngineProjection.
bool g_haveLastAim = false;
Vec3f g_lastAimPoint;

// The first camera update is the only place the engine's own conventions are on
// display with a tracker-free camera, which is what makes the line worth having:
// a build that moved world up, or flipped the matrix column order, says so here
// rather than by leaning the wrong way in front of a player.
void LogCleanBasis(const ViewBasis& basis, engine::Camera* cam) {
    if (g_loggedBasis) return;
    g_loggedBasis = true;
    Log::Line("Clean camera basis: pos (%.2f %.2f %.2f) forward (%.3f %.3f %.3f) "
              "screen-up (%.3f %.3f %.3f) screen-right (%.3f %.3f %.3f)",
              basis.pos.x, basis.pos.y, basis.pos.z, basis.forward.x, basis.forward.y,
              basis.forward.z, basis.up.x, basis.up.y, basis.up.z, basis.right.x, basis.right.y,
              basis.right.z);
    const float upAlignment = Dot(basis.up, kWorldUp);
    if (std::fabs(upAlignment) < 0.5f) {
        Log::Line("WARN: the camera's up axis is %.2f aligned with the world up this mod "
                  "assumes (0,1,0). A lean will not be level. Check the engine's world axes.",
                  static_cast<double>(upAlignment));
    }
    Log::Line("Camera reports FOV %.2f deg, aspect %.4f, near clip %.3f m",
              static_cast<double>(engine::CameraFov(cam)),
              static_cast<double>(engine::CameraAspect(cam)),
              static_cast<double>(engine::CameraClipNear(cam)));
}

// Logged once off the camera rather than off a pose, so the numbers are in the
// log without a tracker connected and without loading a save.
void LogTangents(const FrameTangents& t, engine::Camera* cam) {
    if (g_loggedTangents) return;
    g_loggedTangents = true;
    const float* proj = engine::CameraProjection(cam);
    if (!proj) return;
    Log::Line("Projection: m0 %.4f m5 %.4f -> tan(half h) %.4f tan(half v) %.4f, "
              "implied FOV %.1f x %.1f deg (engine reports %.1f, aspect %.4f)",
              static_cast<double>(proj[0]), static_cast<double>(proj[5]),
              static_cast<double>(t.tan_half_h), static_cast<double>(t.tan_half_v),
              static_cast<double>(2.0f * std::atan(t.tan_half_h) * kRadToDeg),
              static_cast<double>(2.0f * std::atan(t.tan_half_v) * kRadToDeg),
              static_cast<double>(engine::CameraFov(cam)),
              static_cast<double>(engine::CameraAspect(cam)));
}

// How much the head pose is scaled so it moves the picture as far as it would
// at the game's un-zoomed FOV. Exactly 1.0 in ordinary play; below 1.0 in a
// cutscene or aim zoom; a little above it while sprinting widens the view.
//
// Both tangents are vertical: the live one is the projection's own element 5,
// and CameraDefaultFOV is on the axis GetFOV reports, which the first-frame
// Projection line shows matching element 5.
float ZoomFactor(const FrameTangents& t) {
    float baseDeg = 0.0f;
    if (!t.valid || !fov_reference::BaseVerticalFovDegrees(baseDeg)) {
        if (!g_loggedNoZoomReference) {
            g_loggedNoZoomReference = true;
            Log::Line("WARN: no un-zoomed FOV to compare against yet; head movement is not "
                      "zoom-compensated until there is (logged once)");
        }
        return 1.0f;
    }
    const float tanBase = std::tan(baseDeg * 0.5f / kRadToDeg);
    const float factor = cameraunlock::camera::FovZoomFactor(t.tan_half_v, tanBase);
    const float liveDeg = 2.0f * std::atan(t.tan_half_v) * kRadToDeg;
    if (!g_loggedZoomBasis) {
        g_loggedZoomBasis = true;
        Log::Line("Zoom compensation: live vertical FOV %.2f deg (projection element 5), "
                  "un-zoomed CameraDefaultFOV %.2f deg (vertical, degrees), factor %.4f",
                  static_cast<double>(liveDeg), static_cast<double>(baseDeg),
                  static_cast<double>(factor));
    }
    return factor;
}

void ReportLeanClampState() {
    const bool contact = g_leanClamp.InContact();
    const bool failed = g_leanClamp.LastQueryFailed();
    if (contact != g_lastLeanContact || failed != g_lastLeanQueryFailed) {
        Log::Line("Lean clamp: %s, world query %s", contact ? "holding the eye short" : "clear",
                  failed ? "NOT RUNNING" : "running");
        g_lastLeanContact = contact;
        g_lastLeanQueryFailed = failed;
    }
}

// What the ENGINE made of last frame's aim point, in NDC.
//
// PointToScreen goes through the combined matrix the engine built from the
// vectors this hook wrote on the PREVIOUS call, so on entry - before anything is
// written this frame - it reports where that point actually rendered. Turning
// its pixels back into NDC gives a number that can be held against the mod's own
// projection of the same point, and the two agree only if the frame tangents and
// the measured screen convention are both right.
void CheckAgainstEngineProjection(engine::Camera* cam, diagnostics::FrameTrace& trace) {
    if (!g_haveLastAim) return;
    // Read every time rather than cached: a resolution change mid-session would
    // otherwise skew every reading after it.
    const float width = static_cast<float>(engine::ScreenWidth());
    const float height = static_cast<float>(engine::ScreenHeight());
    if (!(width > 0.0f) || !(height > 0.0f)) return;
    const engine::Vec2 px = engine::PointToScreen(cam, FromVec(g_lastAimPoint));
    if (!std::isfinite(px.x) || !std::isfinite(px.y)) return;
    trace.engine_ndc_x = px.x / width * 2.0f - 1.0f;
    trace.engine_ndc_y = 1.0f - px.y / height * 2.0f;
    trace.engine_check_valid = true;
}

// A posed frame with no aim point to put the crosshair on.
void HideReticle() {
    hud_crosshair::PublishAim(false, 0.0f, 0.0f);
    g_haveLastAim = false;
}

// A frame that applies no head pose, so the game's crosshair goes back to where
// the game put it. The clamp is dropped too, or it carries the previous room's
// wall into whatever the next lean is taken against.
void StandDown(const diagnostics::FrameTrace& trace) {
    g_leanClamp.Reset();
    hud_crosshair::PublishCentred();
    flashlight::PublishView(trace.clean, trace.clean);
    g_haveLastAim = false;
    diagnostics::Publish(trace);
}

// Where the round goes, projected into the frame that is about to be drawn.
//
// The depth is the live impact point, never a fixed convergence distance: with a
// lean the render eye and the shot eye are different points, and a fixed depth
// puts the mark on the shot at exactly one range and splays either side of it.
void PublishReticle(const ViewBasis& clean, const ViewBasis& rendered, const FrameTangents& t,
                    diagnostics::FrameTrace& trace) {
    g_haveLastAim = false;

    const TraceHit hit = world_query::GameAimHit();
    trace.aim_queried = hit.queried;
    trace.aim_blocked = hit.blocked;
    trace.aim_distance = hit.distance;
    trace.aim_point = hit.point;
    if (!hit.queried) {
        // No aim trace is not a clear line of fire, so the mark is hidden rather
        // than left standing on a depth nothing measured.
        if (++g_missingAimFrames >= kMissingAimFramesToReport && !g_loggedNoQuery) {
            g_loggedNoQuery = true;
            Log::Line("WARN: the game is not making its aim trace, so the crosshair has no impact "
                      "depth to sit at and stays hidden (logged once)");
        }
        HideReticle();
        return;
    }

    g_missingAimFrames = 0;
    const ScreenPoint p = hit.blocked
                              ? ProjectWorldPoint(hit.point, rendered, t.tan_half_h, t.tan_half_v)
                              : ProjectDirection(clean.forward, rendered, t.tan_half_h, t.tan_half_v);
    hud_crosshair::PublishAim(OnScreen(p), p.ndc_x, p.ndc_y);
    trace.reticle_visible = OnScreen(p);
    trace.reticle_ndc_x = p.ndc_x;
    trace.reticle_ndc_y = p.ndc_y;
    if (hit.blocked && p.valid) {
        g_lastAimPoint = hit.point;
        g_haveLastAim = true;
    }
}

// The engine can drive the camera more than once per frame; only the first call
// runs the tracking pipeline, the rest reuse its pose.
const FrameSample& SampleOncePerFrame(std::uint64_t nowMicros) {
    if (nowMicros - g_lastSampleMicros >= kSameFrameMicros) {
        g_lastSampleMicros = nowMicros;
        g_frameSample = g_tracking->SampleFrame();
    }
    return g_frameSample;
}

EnginePose PoseFromSample(const FrameSample& sample) {
    return EnginePoseFromTracker(
        sample.has_rotation ? sample.yaw : 0.0f, sample.has_rotation ? sample.pitch : 0.0f,
        sample.has_rotation ? sample.roll : 0.0f, sample.has_position ? sample.pos_x : 0.0f,
        sample.has_position ? sample.pos_y : 0.0f, sample.has_position ? sample.pos_z : 0.0f);
}

// The sweep starts at the clean eye - the position the game itself put the
// camera at - so the clamp never reads back a position already inside a wall.
Vec3f ClampLeanAgainstWorld(const ViewBasis& clean, const Vec3f& lean, std::uint64_t nowMicros,
                            diagnostics::FrameTrace& trace) {
    const cameraunlock::math::Vec3 eye(clean.pos.x, clean.pos.y, clean.pos.z);
    const cameraunlock::math::Vec3 desired(lean.x, lean.y, lean.z);
    // Measured against the previous frame that applied a lean, not against the
    // pose sample: they coincide on the first call of a frame and are zero
    // apart on any repeat, which would freeze the clamp's release ease.
    float dt = g_lastAppliedMicros ? static_cast<float>(nowMicros - g_lastAppliedMicros) * 1e-6f
                                   : kNominalFrameDtSec;
    if (!(dt > 0.0f) || dt > kMaxLeanDtSec) dt = kNominalFrameDtSec;
    const cameraunlock::math::Vec3 allowed =
        g_leanClamp.Apply(eye, desired, dt, &lean_trace::Query, &g_leanContext);
    ReportLeanClampState();
    trace.lean_contact = g_leanClamp.InContact();
    trace.lean_query_failed = g_leanClamp.LastQueryFailed();
    return {allowed.x, allowed.y, allowed.z};
}

void Detour(engine::Camera* thiz, const engine::Vec3* forward, const engine::Vec3* up,
            const engine::Vec3* pos) {
    perf_probe::Scope perf(perf_probe::kCamera);
    g_updates.fetch_add(1, std::memory_order_relaxed);

    if (!forward || !up || !pos) {
        perf.Pause();
        g_original(thiz, forward, up, pos);
        return;
    }

    // Only the camera the level is actually presenting. The engine drives this
    // entry point for every camera it owns - reflections, cutscene rigs, the
    // front end's own - and moving one of those moves something the player is not
    // looking through.
    engine::Level* level = engine::ActiveLevel();
    if (!level || thiz != engine::ActiveCamera(level)) {
        perf.Pause();
        g_original(thiz, forward, up, pos);
        return;
    }
    const std::uint64_t now = cameraunlock::time::QpcNowMicros();
    if (now - g_lastFrameMicros >= kSameFrameMicros) perf_probe::NoteFrame(now);
    g_lastFrameMicros = now;

    if (level != g_lastLevel) {
        g_lastLevel = level;
        world_query::ForgetContext();
    }

    if (!g_calibrated) g_calibrated = CalibrateScreenConvention(thiz, g_convention);

    const GateState gate = EvaluateGate(level, g_calibrated);
    if (gate.reason == GateReason::Loading) world_query::ForgetContext();
    g_gateReason.store(static_cast<int>(gate.reason), std::memory_order_relaxed);
    g_multiplayer.store(gate.multiplayer, std::memory_order_relaxed);

    diagnostics::FrameTrace trace;
    trace.gate = GateReasonText(gate.reason);
    trace.multiplayer = gate.multiplayer;
    trace.peers = gate.peers;
    if (diagnostics::IsVerbose()) CheckAgainstEngineProjection(thiz, trace);

    const ViewBasis clean = BasisFromEngine(ToVec(*pos), ToVec(*forward), ToVec(*up), g_convention);
    world_query::NoteCamera(clean.pos, clean.forward);

    trace.clean = clean;
    trace.rendered = clean;

    const bool gateOpen = gate.in_gameplay || (g_cfg.ignore_gameplay_gate && g_calibrated);
    if (!gateOpen || !g_tracking->IsEnabled()) {
        StandDown(trace);
        perf.Pause();
        g_original(thiz, forward, up, pos);
        return;
    }

    LogCleanBasis(clean, thiz);

    const FrameTangents tangents = ReadFrameTangents(thiz);
    if (tangents.valid) LogTangents(tangents, thiz);
    trace.tan_half_h = tangents.tan_half_h;
    trace.tan_half_v = tangents.tan_half_v;
    const float zoom = ZoomFactor(tangents);

    const FrameSample& sample = SampleOncePerFrame(now);

    if (!sample.has_rotation && !sample.has_position) {
        StandDown(trace);
        perf.Pause();
        g_original(thiz, forward, up, pos);
        return;
    }

    // Scaled before the camera write, so the rendered basis, the lean clamp and
    // the reticle projection all describe the camera the player looks through.
    const EnginePose pose = ScalePoseForZoom(PoseFromSample(sample), zoom);

    ViewBasis rendered = ApplyHeadRotation(clean, pose, g_tracking->IsWorldSpaceYaw());

    Vec3f lean = LeanWorldOffset(clean, pose);
    if (g_cfg.collision_enabled) lean = ClampLeanAgainstWorld(clean, lean, now, trace);
    rendered.pos = Add(clean.pos, lean);

    trace.pose = pose;
    trace.rendered = rendered;
    flashlight::PublishView(clean, rendered);

    if (tangents.valid) {
        PublishReticle(clean, rendered, tangents, trace);
    } else {
        HideReticle();
    }

    const engine::Vec3 outForward = FromVec(EngineForwardFromBasis(rendered, g_convention));
    const engine::Vec3 outUp = FromVec(EngineUpFromBasis(rendered, g_convention));
    const engine::Vec3 outPos = FromVec(rendered.pos);

    g_lastAppliedMicros = now;
    trace.posed = true;
    diagnostics::Publish(trace);
    g_posed.fetch_add(1, std::memory_order_relaxed);
    perf.Pause();
    g_original(thiz, &outForward, &outUp, &outPos);
}

}  // namespace

bool InstallCameraHook(TrackingRuntime& tracking, const Config& cfg) {
    g_tracking = &tracking;
    g_cfg = cfg;
    g_leanContext.standoff = cfg.lean_clamp.skin;
    g_leanClamp.SetSettings(cfg.lean_clamp);

    g_target = engine::FromForwardUpPosTarget();
    if (!g_target) return false;

    MH_STATUS status = MH_CreateHook(g_target, reinterpret_cast<LPVOID>(&Detour),
                                     reinterpret_cast<LPVOID*>(&g_original));
    if (status == MH_OK) status = MH_EnableHook(g_target);
    if (status != MH_OK) {
        Log::Line("ERROR: hooking IBaseCamera::FromForwardUpPos @ %p failed: %s", g_target,
                  MH_StatusToString(status));
        MH_RemoveHook(g_target);
        g_target = nullptr;
        return false;
    }
    Log::Line("Camera hook installed @ %p", g_target);
    return true;
}

void RemoveCameraHook() {
    if (!g_target) return;
    MH_DisableHook(g_target);
    MH_RemoveHook(g_target);
    g_target = nullptr;
}

unsigned long long CameraUpdateCount() { return g_updates.load(std::memory_order_relaxed); }
unsigned long long PosedFrameCount() { return g_posed.load(std::memory_order_relaxed); }

std::string DescribeCameraState() {
    std::string out = GateReasonText(static_cast<GateReason>(g_gateReason.load(std::memory_order_relaxed)));
    if (g_multiplayer.load(std::memory_order_relaxed)) out += " [networked session]";
    return out;
}

}  // namespace DyingLightHeadTracking
