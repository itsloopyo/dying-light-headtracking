#pragma once

#include "view_pose.h"

#include <string>

namespace DyingLightHeadTracking::diagnostics {

// What the camera hook did on one frame. Published every frame the hook runs and
// consumed by the verbose trace, so the trace costs one struct copy on a frame
// nobody is reading.
struct FrameTrace {
    bool posed = false;
    const char* gate = "";
    bool multiplayer = false;
    unsigned int peers = 0;

    EnginePose pose;
    ViewBasis clean;
    ViewBasis rendered;

    float tan_half_h = 0.0f;
    float tan_half_v = 0.0f;

    bool aim_queried = false;
    bool aim_blocked = false;
    float aim_distance = 0.0f;
    Vec3f aim_point;

    bool reticle_visible = false;
    float reticle_ndc_x = 0.0f;
    float reticle_ndc_y = 0.0f;

    bool lean_contact = false;
    bool lean_query_failed = false;

    // Where the ENGINE put last frame's aim point, in NDC, worked back from its
    // own PointToScreen on the frame it actually rendered. Compared against the
    // mod's own projection of the same point, it is the one check that the
    // tangents and the screen convention are right - and it is not circular,
    // because the engine projects through the combined matrix it built for
    // itself while the mod projects through the basis it wrote and the
    // projection matrix's diagonal.
    bool engine_check_valid = false;
    float engine_ndc_x = 0.0f;
    float engine_ndc_y = 0.0f;
};

// Starts the verbose trace thread. Does nothing when verbose is false, which is
// the shipped default.
void Start(bool verbose, const std::wstring& triggerPath);
void Stop();

bool IsVerbose();

// Called from the camera hook on every frame it runs.
void Publish(const FrameTrace& trace);

}  // namespace DyingLightHeadTracking::diagnostics
