#include "ball.hpp"
#include <algorithm>
#include <cmath>

namespace pokemon_battle::field {
namespace {
int duration(BallStage s) {
    switch(s) {
    case BallStage::Flight: return 18;
    case BallStage::Open: case BallStage::Close: case BallStage::EscapeOpen: case BallStage::EscapeClose: return 8;
    case BallStage::Transfer: case BallStage::Escape: return 24;
    case BallStage::Drop: return 22;
    case BallStage::Shake: return 72;
    case BallStage::Success: return 26;
    case BallStage::Return: return 18;
    default: return 1;
    }
}
float ease(float t) {return t*t*(3-2*t);}
}
void BallSequence::begin(BallKind kind) {kind_=kind;enter(kind==BallKind::Recall?BallStage::Open:BallStage::Held);}
void BallSequence::release() {if(stage_==BallStage::Held)enter(BallStage::Flight);}
void BallSequence::cancel() {enter(BallStage::None);}
void BallSequence::resolve(bool caught) {if(stage_==BallStage::Resolve)enter(caught?BallStage::Success:BallStage::EscapeOpen);}
bool BallSequence::tick(bool paused) {
    if(paused || !active() || stage_==BallStage::Held || stage_==BallStage::Resolve)return false;
    if(++ticks_<duration(stage_))return false;
    switch(stage_) {
    case BallStage::Flight: enter(BallStage::Open);break;
    case BallStage::Open: enter(BallStage::Transfer);break;
    case BallStage::Transfer: enter(BallStage::Close);break;
    case BallStage::Close: enter(kind_==BallKind::Capture?BallStage::Drop:kind_==BallKind::Summon?BallStage::Return:BallStage::Done);break;
    case BallStage::Drop: enter(BallStage::Shake);break;
    case BallStage::Shake: enter(BallStage::Resolve);return true;
    case BallStage::EscapeOpen: enter(BallStage::Escape);break;
    case BallStage::Escape: enter(BallStage::EscapeClose);break;
    default: enter(BallStage::Done);break;
    }
    return false;
}
BallFrame BallSequence::frame() const {
    BallFrame out;
    if(stage_==BallStage::None || stage_==BallStage::Done)return out;
    out.show_ball=true;out.progress=std::clamp(float(ticks_)/duration(stage_),0.f,1.f);
    const auto t=out.progress,s=ease(t);
    out.scale=kind_==BallKind::Summon?0.f:1.f;
    switch(stage_) {
    case BallStage::Flight:out.spin=t*9;out.wobble=t*5;break;
    case BallStage::Open:out.opening=s;out.glow=s;break;
    case BallStage::Transfer:
        out.opening=1;out.glow=1;out.transfer=t;out.emerging=kind_==BallKind::Summon;
        out.scale=out.emerging?s:1-s;out.opacity=.15f+.85f*out.scale;break;
    case BallStage::Close:out.opening=1-s;out.glow=1-s;out.scale=kind_==BallKind::Summon?1.f:0.f;break;
    case BallStage::Drop:out.scale=0;out.wobble=std::sin(t*13)*(1-t)*.4f;break;
    case BallStage::Shake: {
        out.scale=0;const float beat=std::fmod(float(ticks_),24.f)/24;
        out.wobble=beat<.65f?std::sin(beat/.65f*6.2831853f)*.38f:0;
        out.glow=beat<.65f?std::sin(beat/.65f*3.14159265f):0;break;
    }
    case BallStage::Resolve:out.scale=0;break;
    case BallStage::Success:out.scale=0;out.glow=1;break;
    case BallStage::EscapeOpen:out.scale=0;out.opening=s;out.glow=1;break;
    case BallStage::Escape:out.scale=s;out.opacity=.15f+.85f*s;out.opening=1;out.transfer=t;out.emerging=true;out.glow=1;break;
    case BallStage::EscapeClose:out.scale=1;out.opening=1-s;out.glow=1-s;break;
    case BallStage::Return:out.scale=1;out.spin=t*8;break;
    default:break;
    }
    return out;
}
}
