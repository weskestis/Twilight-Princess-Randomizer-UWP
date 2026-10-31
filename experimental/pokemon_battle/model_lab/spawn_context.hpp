#pragma once

#include "mods/svc/actor.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_layer.h"

namespace pokemon_battle::runtime {

inline bool player_scene_ready(const fopAc_ac_c* player) {
    if(!player || dComIfGp_isEnableNextStage()) return false;
    auto* layer=player->layer_tag.layer;
    return layer && layer!=fpcLy_RootLayer() && !fpcLy_IsDeletingMesg(layer);
}

inline ModResult create_in_player_scene(const ActorService* actors,ModContext* context,
    fopAc_ac_c* player,ProfileName profile,const ActorSpawnParams& params,ActorId& id) {
    id=fpcM_ERROR_PROCESS_ID_e;
    if(!actors || !actors->create_actor || !player_scene_ready(player)) return MOD_UNAVAILABLE;

    // UI callbacks run on the root layer. room_num does not change ownership:
    // ActorService passes the current layer to fpcSCtRq_Request. A root-owned
    // actor is drawn by both dScnPly_c and the root traversal; adding its packet
    // twice to J3DDrawBuffer::entryImm creates a cyclic packet list. It also
    // escapes scene teardown. Bind the request to Link's scene, then restore
    // the caller's layer even when the service fails or throws.
    struct RestoreLayer {
        layer_class* previous;
        ~RestoreLayer() { fpcLy_SetCurrentLayer(previous); }
    } restore{fpcLy_CurrentLayer()};
    fpcLy_SetCurrentLayer(player->layer_tag.layer);
    return actors->create_actor(context,profile,&params,&id);
}

} // namespace pokemon_battle::runtime
