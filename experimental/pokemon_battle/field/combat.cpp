#include "combat.hpp"
#include <algorithm>

namespace pokemon_battle::field {
void Combat::tick(bool can_attack,bool paused) {
    began_move=false; began_strike=false;
    if(paused) return;
    if(invincible>0) --invincible;
    if(attack_down>0) --attack_down;
    if(defense_down>0) --defense_down;
    if(phase==Phase::Fainted) { ++timer; return; }
    if(timer>0) { --timer; return; }
    switch(phase) {
    case Phase::Ready:
        if(can_attack) {
            move=choose_move(creature,random); phase=Phase::Windup; timer=23;
            hit_landed=false; began_move=true;
        }
        break;
    case Phase::Windup: phase=Phase::Strike; timer=5; began_strike=true; break;
    case Phase::Strike:
        phase=Phase::Recover; timer=44;
        if(move==Move::Struggle && hit_landed) damage(std::max(1,int(creature.max_hp)/8));
        break;
    case Phase::Recover: case Phase::Hurt: phase=Phase::Ready; timer=14; break;
    case Phase::Fainted: break;
    }
}
bool Combat::damage(int amount) {
    if(amount<=0 || invincible || !creature.hp || phase==Phase::Fainted) return false;
    if(defense_down) amount=amount*3/2;
    creature.hp=static_cast<std::uint16_t>(std::max(0,int(creature.hp)-amount));
    invincible=20; phase=creature.hp?Phase::Hurt:Phase::Fainted;
    timer=creature.hp?16:0; hit_landed=true;
    return true;
}
int Combat::attack_power() const { return attack_down?1:2; }
int Combat::clip() const {
    if(phase==Phase::Fainted) return 10;
    if(phase==Phase::Hurt) return 11;
    if(phase==Phase::Windup || phase==Phase::Strike || phase==Phase::Recover) return move_info(move)->clip;
    return 0;
}
} // namespace pokemon_battle::field
