#include "party.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace pokemon_battle::field {
namespace {
constexpr std::array<MoveInfo,5> moves{{
    {Move::ThunderShock,"ThunderShock",30,7,true},
    {Move::QuickAttack,"Quick Attack",30,9,true},
    {Move::Growl,"Growl",40,5,false},
    {Move::TailWhip,"Tail Whip",30,3,false},
    {Move::Struggle,"Struggle",0,1,true}
}};
std::uint32_t checksum(std::span<const std::uint8_t> data) {
    std::uint32_t crc=0xffffffffu;
    for(auto byte:data) {
        crc^=byte;
        for(int bit=0;bit<8;++bit) crc=(crc>>1)^(0xedb88320u & (0u-(crc&1u)));
    }
    return ~crc;
}
void put(std::vector<std::uint8_t>& bytes,std::uint64_t value,int count) {
    for(int i=0;i<count;++i) { bytes.push_back(static_cast<std::uint8_t>(value&255u)); value>>=8; }
}
std::uint64_t get(std::span<const std::uint8_t> bytes,std::size_t& pos,int count) {
    if(pos+static_cast<std::size_t>(count)>bytes.size()) throw std::runtime_error("Truncated party data.");
    std::uint64_t value=0;
    for(int i=0;i<count;++i) value|=std::uint64_t(bytes[pos++])<<(i*8);
    return value;
}
}
const MoveInfo* move_info(Move move) {
    for(const auto& info:moves) if(info.move==move) return &info;
    return nullptr;
}
bool valid_creature(const Creature& c,bool captured) {
    if(c.species!=25 || c.level<1 || c.level>100 || c.max_hp!=((85u*c.level)/100u+c.level+10u) ||
       c.hp>c.max_hp || (captured ? c.id==0 : c.id!=0)) return false;
    for(std::size_t i=0;i<move_capacity;++i) {
        const auto* info=move_info(c.moves[i]);
        if(!info || c.moves[i]==Move::Struggle || c.pp[i]>info->pp) return false;
        for(std::size_t j=0;j<i;++j) if(c.moves[i]==c.moves[j]) return false;
    }
    return true;
}
bool valid_party(const Party& p) {
    if(p.count>party_capacity || p.balls>ball_limit || p.random==0 || p.next_id==0 ||
       (p.count ? p.selected>=p.count : p.selected!=0)) return false;
    for(std::size_t i=0;i<p.count;++i) {
        if(!valid_creature(p.members[i],true) || p.members[i].id>=p.next_id) return false;
        for(std::size_t j=0;j<i;++j) if(p.members[i].id==p.members[j].id) return false;
    }
    return true;
}
std::uint32_t random_next(std::uint32_t& state) {
    if(state==0) state=0x6d2b79f5u;
    state^=state<<13; state^=state>>17; state^=state<<5;
    return state;
}
Move choose_move(Creature& c,std::uint32_t& random) {
    std::array<std::size_t,move_capacity> available{};
    std::size_t count=0;
    for(std::size_t i=0;i<move_capacity;++i) if(c.pp[i] && move_info(c.moves[i])) available[count++]=i;
    if(!count) return Move::Struggle;
    const auto chosen=available[random_next(random)%count];
    --c.pp[chosen];
    return c.moves[chosen];
}
int capture_chance(const Creature& c) {
    if(!valid_creature(c,false) || !c.hp) return 0;
    // Deliberately authored action-game balance, not Colosseum's capture formula.
    return std::clamp(20+75*(int(c.max_hp)-int(c.hp))/std::max(1,int(c.max_hp)-1),20,95);
}
void heal(Party& p) {
    for(std::size_t i=0;i<p.count;++i) {
        auto& c=p.members[i]; c.hp=c.max_hp;
        for(std::size_t j=0;j<move_capacity;++j) c.pp[j]=move_info(c.moves[j])->pp;
    }
}
std::vector<std::uint8_t> encode_party(const Party& p) {
    if(!valid_party(p)) return {};
    std::vector<std::uint8_t> bytes{'P','K','P','A','R','T','Y',1};
    put(bytes,p.next_id,8); put(bytes,p.random,4); put(bytes,p.balls,2);
    put(bytes,p.count,1); put(bytes,p.selected,1);
    for(std::size_t i=0;i<p.count;++i) {
        const auto& c=p.members[i];
        put(bytes,c.id,8); put(bytes,c.species,2); put(bytes,c.level,1);
        put(bytes,c.hp,2); put(bytes,c.max_hp,2);
        for(std::size_t j=0;j<move_capacity;++j) { put(bytes,static_cast<std::uint16_t>(c.moves[j]),2); put(bytes,c.pp[j],1); }
    }
    put(bytes,checksum(bytes),4);
    return bytes;
}
bool decode_party(std::span<const std::uint8_t> bytes,Party& out,std::string& error) {
    try {
        constexpr std::array<std::uint8_t,8> magic{'P','K','P','A','R','T','Y',1};
        if(bytes.size()<28 || bytes.size()>28+27*party_capacity || !std::equal(magic.begin(),magic.end(),bytes.begin()))
            throw std::runtime_error("Unknown or invalid Pokemon party save. Original data preserved.");
        std::size_t crc_pos=bytes.size()-4;
        if(get(bytes,crc_pos,4)!=checksum(bytes.first(bytes.size()-4))) throw std::runtime_error("Pokemon party checksum failed. Original data preserved.");
        Party p; std::size_t pos=8;
        p.next_id=get(bytes,pos,8); p.random=static_cast<std::uint32_t>(get(bytes,pos,4));
        p.balls=static_cast<std::uint16_t>(get(bytes,pos,2)); p.count=static_cast<std::uint8_t>(get(bytes,pos,1));
        p.selected=static_cast<std::uint8_t>(get(bytes,pos,1));
        if(p.count>party_capacity || bytes.size()!=28+27*std::size_t(p.count)) throw std::runtime_error("Invalid Pokemon party length.");
        for(std::size_t i=0;i<p.count;++i) {
            auto& c=p.members[i]; c.id=get(bytes,pos,8); c.species=static_cast<std::uint16_t>(get(bytes,pos,2));
            c.level=static_cast<std::uint8_t>(get(bytes,pos,1)); c.hp=static_cast<std::uint16_t>(get(bytes,pos,2));
            c.max_hp=static_cast<std::uint16_t>(get(bytes,pos,2));
            for(std::size_t j=0;j<move_capacity;++j) { c.moves[j]=static_cast<Move>(get(bytes,pos,2)); c.pp[j]=static_cast<std::uint8_t>(get(bytes,pos,1)); }
        }
        if(!valid_party(p)) throw std::runtime_error("Invalid Pokemon party member. Original data preserved.");
        out=p; error.clear(); return true;
    } catch(const std::runtime_error& e) { error=e.what(); return false; }
}
void PartySession::activate(const Party& party) {
    ++epoch_; pending_target_=0; active_=valid_party(party); party_=active_?party:Party{};
}
void PartySession::deactivate() { ++epoch_; active_=false; pending_target_=0; party_=Party{}; }
bool PartySession::commit(const Party& candidate) {
    if(!active_) return false;
    const auto bytes=encode_party(candidate);
    if(bytes.empty() || !storage_.write(bytes)) return false;
    party_=candidate; return true;
}
bool PartySession::begin_throw(std::uint64_t target) {
    if(!active_ || !target || pending() || !party_.balls || party_.count==party_capacity) return false;
    auto next=party_; --next.balls;
    if(!commit(next)) return false;
    pending_target_=target; return true;
}
CaptureResult PartySession::finish_throw(std::uint64_t target,std::uint64_t epoch,const Creature& wild) {
    if(!active_ || epoch!=epoch_ || !target || pending_target_!=target || !valid_creature(wild,false) || !wild.hp)
        return CaptureResult::Invalid;
    pending_target_=0;
    if(party_.count==party_capacity) return CaptureResult::Full;
    auto next=party_;
    const bool caught=int(random_next(next.random)%100)<capture_chance(wild);
    if(caught) {
        if(next.next_id==std::numeric_limits<std::uint64_t>::max()) return CaptureResult::Invalid;
        auto captured=wild; captured.id=next.next_id++;
        next.members[next.count++]=captured;
    }
    if(!commit(next)) return CaptureResult::SaveFailed;
    return caught?CaptureResult::Caught:CaptureResult::Escaped;
}
void PartySession::cancel_throw(std::uint64_t target) {
    if(target==0 || pending_target_==target) pending_target_=0;
}
bool PartySession::update_member(const Creature& creature,std::uint64_t epoch) {
    if(!active_ || epoch!=epoch_ || !valid_creature(creature,true)) return false;
    auto next=party_;
    for(std::size_t i=0;i<next.count;++i) if(next.members[i].id==creature.id) {
        if(creature.species!=next.members[i].species || creature.level!=next.members[i].level ||
           creature.moves!=next.members[i].moves) return false;
        next.members[i]=creature; return commit(next);
    }
    return false;
}
bool PartySession::select_next(int direction) {
    if(!active_ || !party_.count || (direction!=1 && direction!=-1)) return false;
    auto next=party_; next.selected=static_cast<std::uint8_t>((int(next.selected)+int(next.count)+direction)%next.count);
    return commit(next);
}
bool PartySession::rest() { auto next=party_; heal(next); return commit(next); }
bool PartySession::restock() { auto next=party_; next.balls=24; return commit(next); }
} // namespace pokemon_battle::field
