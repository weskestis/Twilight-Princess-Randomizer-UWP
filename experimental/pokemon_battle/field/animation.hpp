#pragma once
#include "combat.hpp"
#include "../runtime/model.hpp"

namespace pokemon_battle::field {
// Strides must match assets/animate_locomotion.py at the actor's 100x scale.
inline constexpr float walk_stride = 28.f, run_stride = 40.f;

class Animation {
public:
    // Call once per unpaused simulation tick, after background collision.
    // Distance is the movement that actually happened, not the requested speed.
    bool sample(const runtime::Model& model, const Combat& combat, float distance,
                runtime::Pose& pose, bool advance=true);
    std::size_t clip() const { return clip_; }
    double gait_phase() const { return gait_phase_; }
    double sample_time() const { return sample_time_; }
private:
    runtime::Pose from_;
    std::size_t clip_=static_cast<std::size_t>(-1);
    double gait_phase_=0, idle_time_=0, sample_time_=0;
    int blend_tick_=0;
    bool running_=false;
};

// Turn through the shortest arc. A turn in place precedes travel when the
// destination is behind the actor, instead of sliding sideways or snapping.
std::int16_t turn_toward(std::int16_t angle, std::int16_t target, int limit=2048);
float forward_alignment(std::int16_t angle, std::int16_t target);
}
