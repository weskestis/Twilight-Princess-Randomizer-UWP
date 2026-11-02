#pragma once
#include "party.hpp"
#include <span>

namespace pokemon_battle::field {
struct Species {
    std::uint16_t id;
    const char* name;
    const char* asset;
    std::array<std::uint8_t,2> types;
    std::array<std::uint16_t,6> stats; // HP, Attack, Defense, Sp. Attack, Sp. Defense, Speed
    std::uint8_t catch_rate;
    std::uint16_t base_exp;
    std::array<Move,4> moves;
    std::array<std::uint8_t,5> clips; // faint, hurt, physical, special, gesture
    float scale,origin_y,height,walk_stride,run_stride;
};
std::span<const Species> roster();
const Species* species_info(std::uint16_t id);
std::uint16_t stat_value(std::uint16_t species,std::uint8_t level,std::size_t stat);
Creature make_creature(std::uint16_t species,std::uint8_t level);
} // namespace pokemon_battle::field
