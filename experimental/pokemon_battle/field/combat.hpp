#pragma once
#include "party.hpp"

namespace pokemon_battle::field {
enum class Phase { Ready, Windup, Strike, Recover, Hurt, Fainted };
struct Combat {
    Creature creature{};
    std::uint32_t random=1;
    Phase phase=Phase::Ready;
    Move move=Move::QuickAttack;
    int timer=45;
    int invincible=0;
    int attack_down=0;
    int defense_down=0;
    bool began_move=false;
    bool began_strike=false;
    bool hit_landed=false;
    // Called once per .749 simulation tick (30 Hz), never per presented frame.
    void tick(bool can_attack, bool paused=false);
    bool damage(int amount);
    int attack_power() const;
    int clip() const;
    bool attacking() const { return phase==Phase::Strike && move_info(move)->damaging && !hit_landed; }
};
} // namespace pokemon_battle::field
