#pragma once

#include "view_pose.h"

#include "cameraunlock/ads/ads_fade.h"

namespace DyingLightHeadTracking {

// Raising the sights narrows the player camera. The field of view gamedll hands
// CameraFPPDI each frame is divided by PlayerFppVis's zoom, which is 1.0 with no
// weapon in hand and otherwise comes from the weapon fire controller, so the zoom
// factor the mod already reads off the projection says the sights are up.
// Walking dips that factor to about 0.98 and sprinting lifts it; below this it
// is the aim zoom.
constexpr float kAimZoomFactor = 0.95f;

inline bool IsAimZoom(float zoomFactor) { return zoomFactor < kAimZoomFactor; }

// Sights locked, the default: the lean eases out while the sights are up, so a
// lean never takes the eye off the sight line. True free look leaves the lean on
// the camera through the aim. Rotation is never touched in either mode, so
// raising the sights does not move the view.
class AimLean {
public:
    // Once per posed frame. `aiming` is polled from this frame's zoom, never
    // latched. Toggling the mode mid-aim rides the same fade as the sights do.
    EnginePose Apply(const EnginePose& pose, bool aiming, bool trueFreeLook, unsigned long long nowMs) {
        const float scale = m_fade.Update(aiming && !trueFreeLook, nowMs);
        EnginePose p = pose;
        p.right *= scale;
        p.up *= scale;
        p.forward *= scale;
        return p;
    }

    // Every frame that applies no pose, so the next aim starts from the hip.
    void Reset() { m_fade.Reset(); }

private:
    cameraunlock::ads::AdsFade m_fade;
};

}  // namespace DyingLightHeadTracking
