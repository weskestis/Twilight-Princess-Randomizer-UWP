#include "../field/combat.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <set>

using namespace pokemon_battle::field;
namespace {
void require(bool condition,const char* what) { if(!condition) { std::cerr<<"FAIL: "<<what<<'\n'; std::exit(1); } }
struct Storage final:PartyStorage {
    std::vector<std::uint8_t> bytes;
    bool fail=false;
    int writes=0;
    bool write(std::span<const std::uint8_t> input) override {
        ++writes;
        if(fail) return false;
        bytes.assign(input.begin(),input.end()); return true;
    }
};
Party caught_party() {
    Party p; p.count=1; p.next_id=2; p.members[0].id=1; p.members[0].hp=8; p.members[0].pp[0]=3; return p;
}
void codec() {
    Party initial; std::string error; Party out=caught_party();
    require(valid_party(initial),"valid initial party");
    const auto empty=encode_party(initial);
    require(empty.size()==28 && empty[7]==1 && empty[20]==24,"versioned portable save header");
    require(decode_party(empty,out,error) && out==initial,"empty round trip");
    Party full;
    full.count=6; full.selected=5; full.balls=99; full.next_id=7;
    for(std::size_t i=0;i<6;++i) { full.members[i].id=i+1; full.members[i].hp=static_cast<std::uint16_t>(i); }
    const auto encoded=encode_party(full);
    require(encoded.size()==190 && decode_party(encoded,out,error) && out==full,"six distinct members round trip");
    for(std::size_t i=0;i<encoded.size();++i) {
        auto bad=encoded; bad[i]^=0x40; Party sentinel=full;
        require(!decode_party(bad,sentinel,error) && sentinel==full,"corruption preserves caller state");
        require(!decode_party(std::span(encoded).first(i),sentinel,error),"all truncations rejected");
    }
    auto bad=full; bad.members[5].id=bad.members[0].id;
    require(encode_party(bad).empty(),"duplicate identities rejected");
    bad=full; bad.members[0].hp=33; require(encode_party(bad).empty(),"HP overflow rejected");
    bad=full; bad.members[0].moves[0]=Move::Struggle; require(encode_party(bad).empty(),"fallback move cannot be saved as a learned move");
    bad=full; bad.members[0].pp[0]=31; require(encode_party(bad).empty(),"PP overflow rejected");
    bad=full; bad.selected=6; require(encode_party(bad).empty(),"invalid selection rejected");
    bad=full; bad.next_id=6; require(encode_party(bad).empty(),"future identity must exceed every member");
    std::uint32_t random=123;
    for(int iteration=0;iteration<2000;++iteration) {
        std::vector<std::uint8_t> bytes(random_next(random)%260);
        for(auto& byte:bytes) byte=static_cast<std::uint8_t>(random_next(random));
        auto sentinel=full; require(!decode_party(bytes,sentinel,error) && sentinel==full,"malformed data is transactional");
    }
}
void captures() {
    Storage storage; PartySession session(storage); Creature wild; wild.hp=1;
    require(!session.begin_throw(10),"inactive slot cannot consume balls");
    session.activate(Party{});
    const auto epoch=session.epoch();
    require(session.begin_throw(10),"throw accepted");
    require(session.party().balls==23 && session.pending(),"ball debit persisted before flight");
    require(!session.begin_throw(11) && session.party().balls==23,"only one in-flight throw");
    const auto other=session.finish_throw(11,epoch,wild);
    require(other==CaptureResult::Invalid && session.pending(),"another actor cannot complete the throw");
    auto result=session.finish_throw(10,epoch,wild);
    require(result==CaptureResult::Caught,"deterministic low-HP capture");
    require(session.party().count==1 && session.party().members[0].hp==1,"capture keeps identity health and PP");
    require(session.finish_throw(10,epoch,wild)==CaptureResult::Invalid && session.party().count==1,"duplicate callback cannot capture twice");
    Party reloaded; std::string error;
    require(decode_party(storage.bytes,reloaded,error) && reloaded==session.party(),"party survives save reload");
    require(session.begin_throw(20),"second throw begins");
    session.activate(Party{});
    require(session.finish_throw(20,epoch,wild)==CaptureResult::Invalid && session.party().count==0,"old slot flight cannot write into new slot");
    require(session.begin_throw(21),"new-slot throw begins"); session.cancel_throw();
    require(session.party().balls==23 && !session.pending(),"area change cancels capture without refunding a thrown ball");
    require(session.finish_throw(21,session.epoch(),wild)==CaptureResult::Invalid,"cancelled flight cannot complete");
    require(session.begin_throw(22),"a new throw can start after cancellation");
    session.cancel_throw(21);
    require(session.pending(),"late destruction of the cancelled actor cannot cancel a new throw");
    session.cancel_throw(22); require(!session.pending(),"only the owning actor can cancel its throw");
    auto full=caught_party(); full.count=6; full.next_id=7;
    for(std::size_t i=0;i<6;++i) full.members[i].id=i+1;
    session.activate(full); require(!session.begin_throw(30) && session.party()==full,"full party costs no ball");
    full=Party{}; full.balls=0; session.activate(full); require(!session.begin_throw(30),"empty inventory rejects throw");
    require(capture_chance(Creature{})==20 && capture_chance(wild)==95,"weakened Pokemon easier to capture");
    wild.hp=0; require(capture_chance(wild)==0,"fainted Pokemon cannot be captured");
}
void failure_and_companions() {
    Storage storage; PartySession session(storage); session.activate(caught_party());
    auto before=session.party(); storage.fail=true;
    require(!session.begin_throw(7) && session.party()==before && !session.pending(),"failed debit leaves world and inventory unchanged");
    storage.fail=false; require(session.begin_throw(7),"retry debit succeeds");
    before=session.party(); const auto stored=storage.bytes; storage.fail=true; Creature wild; wild.hp=1;
    require(session.finish_throw(7,session.epoch(),wild)==CaptureResult::SaveFailed,"capture write failure reported");
    require(session.party()==before && storage.bytes==stored && !session.pending(),"failed capture grants no member and preserves saved data");
    storage.fail=false;
    auto buddy=before.members[0]; buddy.hp=0; buddy.pp[0]=0;
    require(session.update_member(buddy,session.epoch()),"companion HP and PP persisted");
    require(session.party().members[0].hp==0,"fainted companion is retained in party");
    const auto old_epoch=session.epoch(); session.activate(caught_party());
    require(!session.update_member(buddy,old_epoch) && session.party().members[0].hp==8,"stale companion from previous save rejected");
    buddy.id=999; require(!session.update_member(buddy,session.epoch()),"unowned member rejected");
    require(session.rest() && session.party().members[0].hp==32 && session.party().members[0].pp[0]==30,"rest restores HP and PP");
    require(session.select_next(1) && session.party().selected==0,"single member selection wraps");
    session.deactivate(); require(!session.rest() && !session.restock(),"inactive save cannot be mutated");
}
void realtime_combat() {
    Creature mon; std::uint32_t random=9876; std::set<Move> chosen;
    for(int i=0;i<130;++i) chosen.insert(choose_move(mon,random));
    require(chosen.size()==4 && std::all_of(mon.pp.begin(),mon.pp.end(),[](auto pp){return pp==0;}),"random moves exhaust exactly their learned PP");
    require(choose_move(mon,random)==Move::Struggle,"empty move pool uses Struggle");
    Combat combat; combat.timer=0;
    combat.tick(false); require(combat.phase==Phase::Ready,"no attack without eligible target");
    combat.tick(true); require(combat.phase==Phase::Windup && combat.began_move,"attack has readable windup");
    const auto pp=combat.creature.pp; const auto timer=combat.timer;
    for(int i=0;i<300;++i) combat.tick(true,true);
    require(combat.timer==timer && combat.creature.pp==pp && !combat.attacking(),"pause does not age cooldown or consume PP");
    for(int i=0;i<24;++i) combat.tick(true);
    require(combat.phase==Phase::Strike && combat.began_strike,"strike after 24 simulation ticks");
    require(combat.damage(4) && combat.creature.hp==28 && combat.phase==Phase::Hurt,"weapon hit interrupts attack");
    require(!combat.damage(4) && combat.creature.hp==28,"same attack cannot damage every frame");
    for(int i=0;i<20;++i) combat.tick(false);
    require(combat.damage(100) && combat.creature.hp==0 && combat.phase==Phase::Fainted,"lethal damage saturates at zero");
    const auto faint_pp=combat.creature.pp;
    for(int i=0;i<100;++i) combat.tick(true);
    require(combat.creature.pp==faint_pp && !combat.attacking(),"fainted actor never attacks");
    Combat debuffed; debuffed.defense_down=300; require(debuffed.damage(4) && debuffed.creature.hp==26,"Tail Whip affects incoming damage");
    debuffed.attack_down=300; require(debuffed.attack_power()==1,"Growl affects outgoing damage");
}
}
int main() {
    codec(); captures(); failure_and_companions(); realtime_combat();
    std::cout<<"PASS: encounter combat, PP, six-member capture, slot epochs, failed writes, reload, corruption, recall/rest state\n";
}
