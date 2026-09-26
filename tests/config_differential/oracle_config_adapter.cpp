// Compiled into the config oracle library only, with `cameraunlock` and `DyingLightHeadTracking`
// renamed, so "config.h" here is the published build's (oracle/src/config.h).
#include "config.h"
#include "oracle_adapter.h"

namespace dlht_oracle_view {

OracleResult RunOracle(const std::string& path) {
    DyingLightHeadTracking::Config c;
    const bool loaded = c.LoadOrCreate(path.c_str());
    OracleConfig o{};
    o.enabled_on_startup = c.enabled_on_startup;
    o.udp_port = c.udp_port;
    o.data_freshness_ms = c.data_freshness_ms;
    o.world_space_yaw = c.world_space_yaw;
    o.show_reticle = c.show_reticle;
    o.local_smoothing = c.local_smoothing;
    o.remote_smoothing = c.remote_smoothing;
    o.position_enabled = c.position_enabled;
    o.pos_limit_x = c.pos_limit_x;
    o.pos_limit_y = c.pos_limit_y;
    o.pos_limit_y_down = c.pos_limit_y_down;
    o.pos_limit_z = c.pos_limit_z;
    o.pos_limit_z_back = c.pos_limit_z_back;
    o.verbose = c.verbose;
    o.ignore_gameplay_gate = c.ignore_gameplay_gate;
    o.collision_enabled = c.collision_enabled;
    o.collision_radius = c.collision_radius;
    o.collision_release_smoothing = c.collision_release_smoothing;
    o.vk_toggle = c.vk_toggle;
    o.vk_cycle_mode = c.vk_cycle_mode;
    o.vk_yaw_mode = c.vk_yaw_mode;
    o.vk_reticle = c.vk_reticle;
    o.chord_toggle = c.chord_toggle;
    o.chord_cycle_mode = c.chord_cycle_mode;
    o.chord_yaw_mode = c.chord_yaw_mode;
    o.chord_reticle = c.chord_reticle;
    return {loaded, o};
}

}  // namespace dlht_oracle_view
