#include "../field/ball.hpp"
#include "../field/combat.hpp"
#include "../field/roster.hpp"
#include "../runtime/effects.hpp"
#include "runtime_fixture.hpp"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <set>
using namespace pokemon_battle;
using fixture::require;
namespace {
struct Storage:field::PartyStorage {
    bool fail=false;int writes=0;std::vector<std::uint8_t> bytes;
    bool write(std::span<const std::uint8_t> b) override {++writes;if(fail)return false;bytes.assign(b.begin(),b.end());return true;}
};
fixture::Bytes effect_fixture() {
    fixture::Bytes b{'D','P','F','X','0','0','0','1'};fixture::integer(b,0);fixture::integer(b,0);fixture::integer(b,16);
    for(auto name:effects::image_names) {
        char text[32]{};std::strcpy(text,name);b.insert(b.end(),text,text+32);
        for(auto n:{4u,4u,64u})fixture::integer(b,n);
        b.insert(b.end(),64,255);
    }
    fixture::checksum(b);return b;
}
void balls() {
    using namespace field;
    for(const auto kind:{BallKind::Capture,BallKind::Summon,BallKind::Recall})for(bool caught:{false,true}) {
        BallSequence ball;ball.begin(kind);
        const auto initial=ball.stage();for(int i=0;i<300;++i)require(!ball.tick(true),"pause never requests a capture");
        require(ball.stage()==initial && ball.frame().progress==0,"pause freezes transfer");
        if(kind!=BallKind::Recall) {
            for(int i=0;i<300;++i)ball.tick();
            require(ball.stage()==BallStage::Held,"hand release follows real animation, not a timer");ball.release();
        }
        int resolutions=0,frames=0;bool hidden=false,open=false,transfer=false,escape=false;
        while(ball.active() && ++frames<400) {
            const auto f=ball.frame();require(f.scale>=0 && f.scale<=1 && f.opacity>=0 && f.opacity<=1 && f.opening>=0 && f.opening<=1,"bounded ball presentation");
            hidden|=f.scale==0;open|=f.opening==1;transfer|=f.transfer>=0;escape|=ball.stage()==BallStage::Escape;
            if(ball.tick()) {++resolutions;require(kind==BallKind::Capture,"only capture requests a save");
                for(int wait=0;wait<20;++wait)require(!ball.tick(),"resolve notification is not repeated");
                ball.resolve(caught);
            }
        }
        require(frames<400 && hidden && open && transfer,"complete animated ball lifecycle");
        require(resolutions==(kind==BallKind::Capture?1:0),"exactly one capture result");
        require(escape==(kind==BallKind::Capture && !caught),"failed captures visibly release their Pokemon");
        ball.resolve(true);require(ball.stage()==BallStage::Done,"late capture results cannot restart a sequence");
        ball.cancel();require(!ball.active() && !ball.frame().show_ball,"cancel cleans up presentation");
    }
    BallSequence ball;ball.begin(BallKind::Capture);ball.release();ball.tick();ball.cancel();
    for(int i=0;i<300;++i)require(!ball.tick(),"cancelled scene cannot resolve later");
}
void roster_and_saves(const std::filesystem::path& assets) {
    using namespace field;
    require(roster().size()==20,"20 species");std::set<std::uint16_t> ids;std::set<Move> moves;
    Storage storage;PartySession session(storage);session.activate(Party{});storage.fail=true;
    require(!session.choose_starter(4) && session.party().count==0,"failed starter write is transactional");storage.fail=false;
    require(!session.choose_starter(6) && session.choose_starter(4) && !session.choose_starter(7),"choose one supported starter per save");
    for(const auto& species:roster()) {
        require(ids.insert(species.id).second,"unique species IDs");
        for(int level:{1,12,50,100}) {
            auto mon=make_creature(species.id,static_cast<std::uint8_t>(level));require(valid_creature(mon,false),"species stats and four moves valid at every level");
            Party party;party.count=1;party.next_id=2;mon.id=1;party.members[0]=mon;
            Party copy;std::string error;require(decode_party(encode_party(party),copy,error) && copy==party,"all species save round trips");
        }
        auto mon=make_creature(species.id,12);Combat combat;combat.creature=mon;
        for(auto move:mon.moves) {
            moves.insert(move);combat.move=move;combat.phase=Phase::Windup;
            require(move_info(move) && combat.clip()>=0,"every move has a battle clip");
        }
        if(!assets.empty()) {
            char filename[20];std::snprintf(filename,sizeof(filename),"%03u.dpkm",unsigned(species.id));
            auto bytes=fixture::read((assets/filename).string().c_str());runtime::Model model;std::string error;
            require(model.load(bytes,error),error.c_str());require(model.bone_count()>0,"compact skeletal roster");
            for(const auto index:species.clips)require(index<model.clips().size(),"species animation binding exists");
            require(model.clips()[species.clips[0]].name.ends_with("_Faint") && model.clips()[species.clips[1]].name.ends_with("_Damage"),"faint/hurt bindings are correct");
            bool idle=false,walk=false,run=false;
            for(const auto& clip:model.clips()) {idle|=clip.name.ends_with("_Idle");walk|=clip.name.ends_with("_FieldWalk");run|=clip.name.ends_with("_FieldRun");}
            require(idle && walk && run,"all species have idle/walk/run");
            for(auto move:mon.moves) {combat.move=move;require(static_cast<std::size_t>(combat.clip())<model.clips().size(),"every move's bound clip exists in shipped pack");}
        }
    }
    for(auto id:{4,5,6,92,93,94})require(ids.contains(static_cast<std::uint16_t>(id)),"complete Charizard and Gengar lines");
    require(moves.size()==37,"37 learned moves plus Struggle");
    Combat pika;pika.creature=make_creature(25,12);pika.move=Move::ThunderShock;
    require(pika.damage_against(make_creature(7,12))>pika.damage_against(make_creature(1,12)),"electric super effectiveness versus resistance");
    pika.move=Move::QuickAttack;require(pika.damage_against(make_creature(94,12))==0,"normal attacks respect Ghost immunity");
    pika.move=Move::Struggle;require(pika.damage_against(make_creature(94,12))>0,"Struggle bypasses type immunity");
    Combat gengar;gengar.creature=make_creature(94,12);gengar.apply_status(Status::Poison);require(!gengar.poison,"Poison types cannot be poisoned");
    pika.apply_status(Status::Sleep);pika.timer=0;for(int i=0;i<74;++i)pika.tick(true);require(pika.phase==Phase::Ready,"sleep prevents attacks");
    pika.tick(true);pika.tick(true);require(pika.began_move,"sleep expires");
}
void effects_tests(const std::filesystem::path& assets) {
    effects::Pack pack;std::string error;auto bytes=effect_fixture();require(pack.load(bytes,error),error.c_str());
    for(std::size_t length=0;length<bytes.size();++length)require(!pack.load(std::span(bytes).first(length),error) && pack.textures().size()==16,"truncated effects preserve old pack");
    for(auto offset:{20u,52u,56u,60u}) {auto bad=bytes;fixture::overwrite(bad,offset,0xffffffffu);fixture::checksum(bad);require(!pack.load(bad,error),"invalid names/dimensions/length rejected");}
    if(!assets.empty())require(pack.load(fixture::read((assets/"effects.dpfx").string().c_str()),error),error.c_str());
    effects::Origins origins;origins.body={0,50,0};origins.mouth={0,60,20};origins.left={-5,62,18};origins.right={5,62,18};origins.tail={0,10,-30};
    const auto finite=[](const effects::Point& p){return std::isfinite(p[0]) && std::isfinite(p[1]) && std::isfinite(p[2]);};
    for(int visual=0;visual<=static_cast<int>(field::Visual::Bubble);++visual) {
        bool visible=false;
        for(int tick=0;tick<90;++tick) {
            effects::Frame frame;effects::move(frame,static_cast<field::Visual>(visual),tick/30.f,origins,{100,55,100},37);
            visible|=!frame.sprites.empty() || !frame.ribbons.empty();
            require(frame.sprites.size()<=256 && frame.ribbons.size()<=192,"effect budget");
            for(const auto& s:frame.sprites)require(finite(s.position) && std::isfinite(s.width+s.height+s.rotation) && s.width>0 && s.height>0 && s.color[3]>=0 && s.color[3]<=1,"finite particle geometry");
            for(const auto& r:frame.ribbons)require(finite(r.from) && finite(r.to) && std::isfinite(r.width) && r.width>0,"finite ribbon geometry");
        }
        require(visible,"every move family has an effect");
    }
    effects::Frame fire,gas,electric;effects::ambient(fire,6,1,origins);effects::ambient(gas,92,1,origins);
    effects::move(electric,field::Visual::Electric,.8f,origins,{100,55,100});
    require(fire.sprites.size()==10 && gas.sprites.size()==14,"tail flame and Gastly gas accompany animated models");
    require(electric.ribbons.size()==72 && electric.sprites.size()>10,"two-cheek branching layered ThunderShock with source sprites");
}
}
int main(int argc,char** argv) {
    try {const std::filesystem::path assets=argc>1?argv[1]:"";balls();roster_and_saves(assets);effects_tests(assets);
        std::cout<<"PASS: 20-species mode, starters, legacy save codec, type damage, status effects, ball phases and particle pack/timelines\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
