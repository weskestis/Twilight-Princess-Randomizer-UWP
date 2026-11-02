#include "roster.hpp"
#include <algorithm>

namespace pokemon_battle::field {
namespace {
constexpr Species entries[]{
#include "roster_data.inc"
};
static_assert(std::size(entries)==20);
}
std::span<const Species> roster() { return entries; }
const Species* species_info(std::uint16_t id) {
    for(const auto& entry:entries) if(entry.id==id)return &entry;
    return nullptr;
}
std::uint16_t stat_value(std::uint16_t species,std::uint8_t level,std::size_t stat) {
    const auto* entry=species_info(species);
    if(!entry || stat>=entry->stats.size() || !level || level>100)return 0;
    return static_cast<std::uint16_t>(((2u*entry->stats[stat]+15u)*level)/100u+(stat==0?level+10u:5u));
}
Creature make_creature(std::uint16_t id,std::uint8_t level) {
    Creature result;
    const auto* species=species_info(id);
    if(!species) { result.species=0;return result; }
    result.species=id;result.level=std::clamp<std::uint8_t>(level,1,100);
    result.max_hp=stat_value(id,result.level,0);result.hp=result.max_hp;
    result.moves=species->moves;
    for(std::size_t i=0;i<result.moves.size();++i)result.pp[i]=move_info(result.moves[i])->pp;
    return result;
}
} // namespace pokemon_battle::field
