#pragma once
#include "dolphin/gx.h"
#include "SSystem/SComponent/c_xyz.h"
#include "../runtime/effects.hpp"

namespace pokemon_battle::runtime {
void draw_health(const cXyz& position,float fraction,bool companion,const Mtx view);
void draw_ball(const cXyz& position,float wobble,const Mtx view,float opening=0,float spin=0,float glow=0);
bool load_effects(std::span<const std::uint8_t> bytes,std::string& error);
void release_effects();
void draw_effects(const effects::Frame& frame,const Mtx view);
}
