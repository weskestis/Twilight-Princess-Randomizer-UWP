#pragma once

namespace pokemon_battle::field {
enum class BallKind { Capture, Summon, Recall };
enum class BallStage { None, Held, Flight, Open, Transfer, Close, Drop, Shake, Resolve, Success, EscapeOpen, Escape, EscapeClose, Return, Done };
struct BallFrame {
    float progress=0,scale=1,opacity=1,opening=0,wobble=0,spin=0,glow=0,transfer=-1;
    bool emerging=false,show_ball=false;
};
// Runs exclusively on the game's 30 Hz simulation clock. Capture persistence
// is requested once at Resolve and acknowledged with resolve(), never in draw.
class BallSequence {
public:
    void begin(BallKind kind);
    void release();
    bool tick(bool paused=false);
    void resolve(bool caught);
    void cancel();
    BallFrame frame() const;
    BallKind kind() const {return kind_;}
    BallStage stage() const {return stage_;}
    bool active() const {return stage_!=BallStage::None && stage_!=BallStage::Done;}
private:
    void enter(BallStage stage) {stage_=stage;ticks_=0;}
    BallKind kind_=BallKind::Capture;
    BallStage stage_=BallStage::None;
    int ticks_=0;
};
}
