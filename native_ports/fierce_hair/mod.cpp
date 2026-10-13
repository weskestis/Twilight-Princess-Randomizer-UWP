// Native Dusklight adapter. Build against the SAME game/SDK ABI as the host.
#include "mods/service.hpp"
#include "mods/svc/hook.hpp"
#include "mods/svc/log.h"
#include "d/actor/d_a_alink.h"
#include "hair_spring.hpp"

DEFINE_MOD();
IMPORT_SERVICE(LogService,svc_log);
IMPORT_SERVICE(HookService,svc_hook);
DEFINE_HOOK(&daAlink_c::setHatAngle,LinkHairUpdate);
DEFINE_HOOK(&daAlink_c::execute,LinkFrame);
namespace {
fierce_hair::Spring spring;
daAlink_c* owner=nullptr;
HookAction before_frame(ModContext*,void* args,void*,void*) {
    auto link=mods::arg<daAlink_c*>(args,0);
    if(!link||link->checkWolf()||owner!=link){spring.reset();owner=link;}
    return HOOK_CONTINUE;
}
void update_hair(ModContext*,void* args,void*,void*) {
    daAlink_c* link=mods::arg<daAlink_c*>(args,0);
    if(!link||link->checkWolf()||!link->mpLinkHatModel){spring.reset();owner=nullptr;return;}
    if(owner!=link){spring.reset();owner=link;}
    if(link->checkNoResetFlg2(daAlink_c::FLG2_STATUS_WINDOW_DRAW)){return;}
    if(link->mpLinkHatModel->getModelData()->getJointNum()!=10){spring.reset();return;}
    // setHatAngle has already supplied wind targets. Head-root tracking adds
    // movement inertia without borrowing the pelvis/skirt skeleton.
    fierce_hair::Input input;
    auto matrix=link->mpLinkHatModel->getBaseTRMtx();
    for(int i=0;i<3;++i)input.head_position[i]=matrix[i][3];
    input.facing=static_cast<float>(link->shape_angle.y)*fierce_hair::kRadiansPerBam;
    for(int i=0;i<5;++i){
        int joint=fierce_hair::kHairJoints[i];
        input.target_x[i]=link->field_0x302c[joint]*fierce_hair::kRadiansPerBam;
        input.target_z[i]=link->field_0x3040[joint]*fierce_hair::kRadiansPerBam;
    }
    auto result=spring.step(input);
    for(int i=0;i<5;++i){
        int joint=fierce_hair::kHairJoints[i];
        link->field_0x302c[joint]=fierce_hair::bam(result.x[i]);
        link->field_0x3040[joint]=fierce_hair::bam(result.z[i]);
    }
    // The native head callback consumes these angles exactly once. Joint 0,
    // hat joints 6-9, Link's body and every skirt bone are untouched.
}
}
extern "C" {
MOD_EXPORT ModResult mod_initialize(ModError*) {
    spring.reset();owner=nullptr;
    auto result=mods::hook::add_pre<LinkFrame>(before_frame);
    if(result==MOD_OK)result=mods::hook::add_post<LinkHairUpdate>(update_hair);
    if(result!=MOD_OK)svc_log->error(mod_ctx,"Fierce hair: setHatAngle hook unavailable");
    return result;
}
MOD_EXPORT ModResult mod_update(ModError*){return MOD_OK;}
MOD_EXPORT ModResult mod_shutdown(ModError*){spring.reset();owner=nullptr;return MOD_OK;}
}
