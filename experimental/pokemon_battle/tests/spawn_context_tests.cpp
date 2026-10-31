// This test is the game host for these fixtures, including on the unpatched
// upstream SDK where the UWP static-built-in visibility option is absent.
#undef DUSK_GAME_DATA
#define DUSK_GAME_DATA
#include "../model_lab/spawn_context.hpp"
#include "JSystem/J3DGraphBase/J3DDrawBuffer.h"
#include "JSystem/J3DGraphBase/J3DPacket.h"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition,const char* message) {
    if(!condition) throw std::runtime_error(message);
}
layer_class root{},scene{},other{};
layer_class* current=&root;
layer_class* request_owner=nullptr;
bool changing_stage=false,throw_create=false;
ModResult create_result=MOD_OK;
unsigned creates=0,draws=0;
ModResult record_spawn_request(ModContext*,ProfileName,const ActorSpawnParams*,ActorId* out) {
    ++creates;
    request_owner=current; // fpcM_Create captures fpcLy_CurrentLayer() in the real engine.
    if(throw_create) throw std::runtime_error("simulated service exception");
    if(create_result==MOD_OK) *out=123;
    return create_result;
}
class TestPacket final : public J3DPacket {
public:
    void draw() override { ++draws; }
};
// dScnPly draws every actor in the global draw queue; fpcM_DrawIterater
// additionally draws processes owned by the root layer. Both submit to the
// same J3DDrawBuffer. entryImm below is compiled from the pinned game source.
bool submit_scene_and_root(J3DDrawBuffer& buffer,TestPacket& packet) {
    buffer.mpBuffer[0]=nullptr;
    buffer.entryImm(&packet,0);
    if(request_owner==&root) buffer.entryImm(&packet,0);
    return packet.getNextPacket()==&packet;
}
}

// Game-boundary fixtures use the real .749 types and service declarations.
// No copied actor/SDK layouts or replacement implementation of entryImm.
BOOL dComIfGp_isEnableNextStage() { return changing_stage; }
layer_class* fpcLy_RootLayer() { return &root; }
layer_class* fpcLy_CurrentLayer() { return current; }
void fpcLy_SetCurrentLayer(layer_class* layer) { current=layer; }
BOOL fpcLy_IsDeletingMesg(layer_class* layer) { return layer->counts.delete_count>0; }
base_process_class::~base_process_class()=default;
fopAc_ac_c::fopAc_ac_c()=default;
fopAc_ac_c::~fopAc_ac_c()=default;
dEvt_info_c::dEvt_info_c()=default;
const J3DLightInfo j3dDefaultLightInfo{};
J3DLightInfo& J3DLightInfo::operator=(const J3DLightInfo&)=default;
int J3DPacket::entry(J3DDrawBuffer*) { return 0; }
void J3DPacket::draw() {}
void J3DDrawBuffer::initialize() { mpBuffer=nullptr; mEntryTableSize=1; }
J3DDrawBuffer::~J3DDrawBuffer()=default;

int main() {
    using namespace pokemon_battle::runtime;
    try {
        fopAc_ac_c player;
        player.layer_tag.layer=&scene;
        ActorService service{};
        service.create_actor=record_spawn_request;
        ActorSpawnParams params{};
        params.room_num=0;
        ActorId id=fpcM_ERROR_PROCESS_ID_e;
        J3DDrawBuffer buffer;
        J3DPacket* entries[1]{};
        buffer.mpBuffer=entries;
        TestPacket packet;

        // Negative control: reproduce the shipped .750 root-owned request.
        require(service.create_actor(nullptr,777,&params,&id)==MOD_OK,"legacy request");
        require(submit_scene_and_root(buffer,packet),"legacy code must reproduce a packet-list cycle");

        for(auto* caller:{&root,&other,&scene}) {
            current=caller;
            for(int room:{-1,0,63}) {
                params.room_num=static_cast<int8_t>(room);
                require(create_in_player_scene(&service,nullptr,&player,777,params,id)==MOD_OK,"scene spawn failed");
                require(id==123 && request_owner==&scene,"spawn did not belong to Link's scene");
                require(current==caller,"caller layer not restored");
                require(!submit_scene_and_root(buffer,packet),"fixed spawn still cycles the draw queue");
                const auto before=draws;
                buffer.drawHead();
                require(draws==before+1,"preview must draw exactly once");
            }
        }
        current=&root;
        create_result=MOD_ERROR;
        require(create_in_player_scene(&service,nullptr,&player,777,params,id)==MOD_ERROR,"service error lost");
        require(current==&root && id==fpcM_ERROR_PROCESS_ID_e,"failed spawn leaked state");
        throw_create=true;
        try { (void)create_in_player_scene(&service,nullptr,&player,777,params,id); }
        catch(const std::runtime_error&) {}
        require(current==&root,"exception did not restore caller layer");
        throw_create=false; create_result=MOD_OK;

        const auto before=creates;
        for(auto* owner:{static_cast<layer_class*>(nullptr),&root}) {
            player.layer_tag.layer=owner;
            require(create_in_player_scene(&service,nullptr,&player,777,params,id)==MOD_UNAVAILABLE,"invalid scene accepted");
        }
        player.layer_tag.layer=&scene;
        scene.counts.delete_count=1;
        require(create_in_player_scene(&service,nullptr,&player,777,params,id)==MOD_UNAVAILABLE,"deleting scene accepted");
        scene.counts.delete_count=0;
        changing_stage=true;
        require(create_in_player_scene(&service,nullptr,&player,777,params,id)==MOD_UNAVAILABLE,"transition accepted");
        changing_stage=false;
        require(create_in_player_scene(&service,nullptr,nullptr,777,params,id)==MOD_UNAVAILABLE,"missing player accepted");
        require(create_in_player_scene(nullptr,nullptr,&player,777,params,id)==MOD_UNAVAILABLE,"missing service accepted");
        service.create_actor=nullptr;
        require(create_in_player_scene(&service,nullptr,&player,777,params,id)==MOD_UNAVAILABLE,"missing callback accepted");
        require(creates==before && current==&root,"rejected spawn entered the engine");
        std::cout<<"PASS: .750 root-spawn packet cycle reproduced; scene-owned spawning draws once; failure/exception restoration; missing/deleting/transitioning scene rejection\n";
    } catch(const std::exception& error) {
        std::cerr<<"FAIL: "<<error.what()<<'\n'; return 1;
    }
}
