#pragma once
#include "dolphin/gx.h"
#include "SSystem/SComponent/c_xyz.h"

namespace pokemon_battle::runtime {
void draw_health(const cXyz& position,float fraction,bool companion,const Mtx view);
void draw_ball(const cXyz& position,float wobble,const Mtx view);
void draw_spark(const cXyz& start,const cXyz& end,const Mtx view);
}
