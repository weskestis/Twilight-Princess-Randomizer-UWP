#include "combat.hpp"
#include "roster.hpp"
#include <algorithm>

namespace pokemon_battle::field {
namespace {
constexpr std::uint8_t effectiveness[18][18]={
#include "type_chart.inc"
};
}
void Combat::tick(bool can_attack,bool paused) {
    began_move=false; began_strike=false;
    if(paused) return;
    if(invincible>0) --invincible;
    if(attack_down>0) --attack_down;
    if(defense_down>0) --defense_down;
    if(accuracy_down>0)--accuracy_down;
    if(confusion>0)--confusion;
    if(paralysis>0)--paralysis;
    if(poison>0) {--poison;if(poison%90==0)damage(std::max(1,int(creature.max_hp)/16));}
    if(sleep>0) {--sleep;return;}
    if(phase==Phase::Fainted) { ++timer; return; }
    if(timer>0) { --timer; return; }
    switch(phase) {
    case Phase::Ready:
        if(can_attack) {
            if(paralysis && random_next(random)%4==0) {timer=15;return;}
            if(confusion && random_next(random)%4==0) {damage(std::max(1,int(creature.max_hp)/12));return;}
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
    sleep=0;
    creature.hp=static_cast<std::uint16_t>(std::max(0,int(creature.hp)-amount));
    invincible=20; phase=creature.hp?Phase::Hurt:Phase::Fainted;
    timer=creature.hp?16:0; hit_landed=true;
    return true;
}
int Combat::attack_power() const {
    const auto* info=move_info(move);
    const int power=info?std::clamp(1+int(info->power)/35,2,5):2;
    return attack_down?std::max(1,power*2/3):power;
}
int Combat::damage_against(const Creature& target) const {
    const auto* info=move_info(move);const auto* source=species_info(creature.species);const auto* other=species_info(target.species);
    if(!info || !source || !other || !info->damaging)return 0;
    if(move==Move::DragonRage)return 40;
    const bool special=info->type>=10;
    const int attack=stat_value(creature.species,creature.level,special?3:1);
    const int defense=std::max(1,int(stat_value(target.species,target.level,special?4:2)));
    int damage=((2*creature.level/5+2)*info->power*attack/defense)/50+2;
    if(attack_down)damage=damage*2/3;
    if(move!=Move::Struggle) {
        if(source->types[0]==info->type || source->types[1]==info->type)damage=damage*3/2;
        int multiplier=effectiveness[info->type][other->types[0]];
        if(other->types[1]!=other->types[0])multiplier=multiplier*effectiveness[info->type][other->types[1]]/10;
        if(!multiplier)return 0;
        damage=damage*multiplier/10;
    }
    return std::max(1,damage);
}
bool Combat::accuracy_roll() {
    const auto* info=move_info(move);
    if(!info)return false;
    int chance=info->accuracy?info->accuracy:100;
    if(accuracy_down)chance=chance*3/4;
    return int(random_next(random)%100)<chance;
}
void Combat::apply_status(Status status) {
    switch(status) {
    case Status::AttackDown:attack_down=300;break;
    case Status::DefenseDown:defense_down=300;break;
    case Status::AccuracyDown:accuracy_down=300;break;
    case Status::Confusion:confusion=180;break;
    case Status::Paralysis:paralysis=300;break;
    case Status::Sleep:sleep=75;break;
    case Status::Poison: {
        const auto* species=species_info(creature.species);
        if(species && species->types[0]!=3 && species->types[1]!=3 && species->types[0]!=8 && species->types[1]!=8)poison=450;
        break;
    }
    case Status::None:break;
    }
}
int Combat::clip() const {
    const auto* species=species_info(creature.species);
    if(!species)return 0;
    if(phase==Phase::Fainted) return species->clips[0];
    if(phase==Phase::Hurt) return species->clips[1];
    if(phase==Phase::Windup || phase==Phase::Strike || phase==Phase::Recover) {
        const auto* info=move_info(move);
        if(!info)return 0;
        if(creature.species==25)return info->clip;
        const bool physical=info->visual==Visual::Impact || info->visual==Visual::Slash ||
            info->visual==Visual::Bite || info->visual==Visual::Dash || info->visual==Visual::Spin || info->visual==Visual::Wind;
        return species->clips[!info->damaging?4:physical?2:3];
    }
    return 0;
}
} // namespace pokemon_battle::field
