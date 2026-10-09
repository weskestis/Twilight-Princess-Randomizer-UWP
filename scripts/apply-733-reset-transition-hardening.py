from __future__ import annotations

import os
import re
from pathlib import Path

ROOT = Path(os.environ["TPR_SRC"])
CONTROL = Path(os.environ.get("TPR_CONTROL", Path(__file__).resolve().parents[1]))
PENDING: dict[str, str] = {}


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8").replace("\r\n", "\n")


def write(relative: str, source: str) -> None:
    PENDING[relative] = source


def once(source: str, old: str, new: str, label: str) -> str:
    count = source.count(old)
    if count != 1:
        raise RuntimeError(f".733 {label}: expected one anchor, found {count}")
    return source.replace(old, new, 1)


def function_span(source: str, signature: str) -> tuple[int, int]:
    # Ignore braces inside strings, characters and comments.
    start = source.index(signature)
    token = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/|[{}]')
    opened = False
    depth = 0
    for match in token.finditer(source, start):
        ch = match.group()
        if ch == "{":
            opened = True
            depth += 1
        elif ch == "}":
            depth -= 1
            if opened and depth == 0:
                return start, match.end()
    raise RuntimeError(f".733 unclosed function: {signature}")


# Queue a native reset once; do not unload code/data while old actors are alive.
path = "src/m_Do/m_Do_Reset.cpp"
source = read(path)
include = '#include "dusk/main.h"'
source = once(source, include, include + '''
#if defined(_UWP)
#include "dusk/ui/ui.hpp"
#include "f_op/f_op_overlap_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#endif''', "reset includes")
payload = (CONTROL / "payloads/xbox-reset-lifecycle-733.inc").read_text(encoding="utf-8")
start, end = function_span(source, "void mDoRst_resetCallBack(int port, void*)")
old_body = source[start:end]
legacy_body = old_body[old_body.index("{") + 1 : -1]
new_function = '''void mDoRst_resetCallBack(int port, void*) {
#if defined(_UWP)
    if (mDoRst::isReset() || s_xboxResetToLauncherPending) {
        return;
    }
    if (port != -1) {
        if (mDoRst::is3ButtonReset()) {
            JUTGamePad::C3ButtonReset::sResetOccurred = false;
            JUTGamePad::setResetCallback(mDoRst_resetCallBack, nullptr);
            return;
        }
        mDoRst::on3ButtonReset();
        mDoRst::set3ButtonResetPort(port);
    }
    cAPICPad_recalibrate();
    // Latch before calling mod code: recursive/duplicate reset events are ignored.
    s_xboxResetToLauncherPending = true;
    mDoRst::onReset();
    dusk::ui::prelaunch_state().returnToPrelaunchOnReset = true;
    dusk::startup_guard::begin_activity("reset.requested");
    const auto* gameMode = dusk::gamemode::getGameModeManager().getCurrentGameMode();
    if (gameMode != nullptr) {
        gameMode->invokeOnGameResetFunction();
    }
    // Let the engine reach a fresh opening scene before presenting the launcher.
    dusk::ui::close_all_documents();
    return;
#else
    if (mDoRst::isReset()) {
        return;
    }
''' + legacy_body + '''
#endif
}'''
source = source[:start] + payload + "\n" + new_function + source[end:]
write(path, source)

path = "include/m_Do/m_Do_Reset.h"
source = once(read(path), "void mDoRst_resetCallBack(int, void*);", '''void mDoRst_resetCallBack(int, void*);
#if defined(_UWP)
bool tpr_xbox_reset_to_launcher_pending() noexcept;
void tpr_xbox_complete_reset_to_launcher();
#endif''', "reset declarations")
write(path, source)

# Skip gameplay audio work during teardown, but drain sound state without stopSync's
# waitSubFrame loop: Xbox DSP callbacks run on the main thread in Pump().
path = "src/m_Do/m_Do_audio.cpp"
source = once(read(path), '''            dusk::startup_guard::stage("audio.gframe-reset-skip");
            return;''', '''            dusk::startup_guard::stage("audio.gframe-reset-skip");
            g_mDoAud_zelAudio.gframeProcess();
            return;''', "reset audio frame")
source = once(source, "bool mDoAud_resetRecover() {", '''bool mDoAud_resetRecover() {
    DUSK_AUDIO_SKIP(true)''', "disabled-audio reset recovery")
write(path, source)

path = "src/Z2AudioLib/Z2AudioMgr.cpp"
source = once(read(path), '#include "dusk/startup_guard.hpp"', '''#include "dusk/startup_guard.hpp"
#include "m_Do/m_Do_Reset.h"''', "Z2 reset include")
source = once(source, "void Z2AudioMgr::gframeProcess() {", '''void Z2AudioMgr::gframeProcess() {
#if defined(_UWP)
    if (mDoRst::isReset()) {
        // resetGame()/hasReset() need both the DSP reset callback and cleared sounds.
        // Keep this frame nonblocking; stopSync waits for DSP work on this same thread.
        dusk::startup_guard::stage("audio.reset-sound-drain");
        if (mResettingFlag) {
            mSoundMgr.stop();
            mSoundMgr.framework();
            framework();
        }
        return;
    }
#endif''', "nonblocking reset sound drain")
write(path, source)

# Render reset DSP subframes without queuing them. This progresses the reset even
# with a paused output voice/full output queue, then preserves the normal audio path.
path = "src/dusk/audio/DuskAudioSystem.cpp"
source = once(read(path), '#include "m_Do/m_Do_Reset.h"', '''#include "m_Do/m_Do_Reset.h"
#include "m_Do/m_Do_audio.h"''', "audio reset flag include")
source = once(source, '''void dusk::audio::Pump() {
#if defined(_UWP)
    if (mDoRst::isReset()) {
        return;
    }
#endif''', '''void dusk::audio::Pump() {
#if defined(_UWP)
    if (mDoRst::isReset()) {
        if (mDoAud_zelAudio_c::isInitFlag() && mDoAud_zelAudio_c::isResetFlag() &&
            OutChannelCount == 2)
        {
            dusk::startup_guard::stage("audio.reset-dsp-progress");
            const u32 countSubframes = JASDriver::getSubFrames();
            JASCriticalSection section;
            JASAudioThread::setDSPSyncCount(countSubframes);
            for (u32 i = 0; i < countSubframes; ++i) {
                const int rendered = RenderAudioSubframe();
                JASAudioThread::snIntCount -= 1;
                if (rendered <= 0) {
                    return;
                }
            }
        }
        return;
    }
#endif''', "reset DSP pump")
write(path, source)

# Keep crash tracing active for the whole transaction, including slow resource loads.
path = "src/dusk/startup_guard.hpp"
source = read(path)
anchor = 'inline void complete_startup() noexcept {'
if source.count(anchor) != 2:
    raise RuntimeError(".733 startup guard platform branches changed")
source = source.replace(anchor, '''inline void keep_activity_alive() noexcept {
    std::scoped_lock lock(detail::mutex);
    detail::traceEnabled.store(true, std::memory_order_relaxed);
    detail::activityAwaitingFrame = true;
    detail::activityFramesRemaining = detail::kActivityTraceFrames;
}

''' + anchor, 1)
source = once(source, "inline void complete_startup() noexcept {}", '''inline void keep_activity_alive() noexcept {}
inline void complete_startup() noexcept {}''', "non-UWP trace stub")
source = source.replace("xbox-startup-stage-732.txt", "xbox-startup-stage-733.txt")
source = once(source, '''        if (input) {
            std::getline(input, detail::previousStage);
        }''', '''        if (input) {
            std::getline(input, detail::previousStage);
        } else {
            // Carry forward the .732 checkpoint when updating after a hardware crash.
            std::ifstream prior(userPath / "xbox-startup-stage-732.txt", std::ios::binary);
            if (prior) {
                std::getline(prior, detail::previousStage);
            }
        }''', ".732 diagnostic migration")
source = source.replace("build=1.4.1.730", "build=1.4.1.733")
write(path, source)

path = "src/m_Do/m_Do_main.cpp"
source = read(path)
source = once(source, "void main01(void) {", '''#if defined(_UWP)
extern "C" int tpr_xbox_scene_change_present() noexcept;
extern "C" int tpr_xbox_create_queue_size() noexcept;
extern "C" int tpr_xbox_delete_queue_size() noexcept;
#endif

void main01(void) {''', "main reset poll declarations")
source = once(source, '''        dusk::startup_guard::stage("main01.frame-begin");''', '''#if defined(_UWP)
        if (tpr_xbox_reset_to_launcher_pending() ||
            tpr_xbox_scene_change_present() != 0 ||
            tpr_xbox_create_queue_size() != 0 || tpr_xbox_delete_queue_size() != 0)
        {
            dusk::startup_guard::keep_activity_alive();
        }
#endif
        dusk::startup_guard::stage("main01.frame-begin");''', "transaction trace renewal")
start = source.index("void main01(void)")
main = once(source[start:], '''        aurora_end_frame();
        advance_xbox_startup_guard();''', '''        aurora_end_frame();
#if defined(_UWP)
        tpr_xbox_complete_reset_to_launcher();
#endif
        advance_xbox_startup_guard();''', "safe end-of-frame reset completion")
source = source[:start] + main
write(path, source)

path = "src/f_op/f_op_scene_mng.cpp"
source = once(read(path), '#include "dusk/logging.h"', '''#include "dusk/logging.h"
#if defined(_UWP)
#include "dusk/startup_guard.hpp"
#endif''', "scene-request journal include")
source = once(source, '''    fpc_ProcID request_id = fopScnRq_Request(2, i_scene, i_procName, NULL, param_3, param_4);''', '''#if defined(_UWP)
    char checkpoint[160];
    std::snprintf(checkpoint, sizeof(checkpoint),
        "scene.change-request:old=%u:target=%d:fade=%d:time=%u",
        i_scene != nullptr ? static_cast<unsigned int>(fpcM_GetID(i_scene)) : 0U,
        static_cast<int>(i_procName), static_cast<int>(param_3),
        static_cast<unsigned int>(param_4));
    dusk::startup_guard::begin_activity(checkpoint);
#endif
    fpc_ProcID request_id = fopScnRq_Request(2, i_scene, i_procName, NULL, param_3, param_4);
#if defined(_UWP)
    dusk::startup_guard::end_activity(
        request_id == fpcM_ERROR_PROCESS_ID_e ? "scene.request-rejected" : "scene.request-accepted");
#endif''', "scene request checkpoint")
write(path, source)

# Do not cancel/unpause an in-flight scene, a scripted event or the old scene's camera.
path = "mods/randomizer/src/session.cpp"
source = read(path)
start, end = function_span(source, "int clearXboxTransitionFades() noexcept")
fn = source[start:end]
fn = once(fn, '''    if (sceneChangePending && activeGameplayScene == nullptr) {''', '''    if (sceneChangePending || tpr_xbox_create_queue_size() != 0 ||
        tpr_xbox_delete_queue_size() != 0 || activeGameplayScene == nullptr ||
        dComIfGp_getPlayer(0) == nullptr ||
        dComIfGp_getCamera(dComIfGp_getPlayerCameraID(0)) == nullptr ||
        dComIfGp_event_runCheck() || dComIfGp_isEnableNextStage() ||
        mDoRst::isReset() || tpr_xbox_reset_to_launcher_pending())
    {''', "transition recovery transaction guard")
source = source[:start] + fn + source[end:]
source = once(source, '''ModResult onGameModeUpdate(void*, ModError*) {
    ui::update();''', '''ModResult onGameModeUpdate(void*, ModError*) {
#if defined(_UWP)
    if (mDoRst::isReset() || tpr_xbox_reset_to_launcher_pending()) {
        return MOD_OK;
    }
#endif
    ui::update();''', "Randomizer reset tick guard")
source = once(source, '''    static int s_xboxTransitionStage = -1;''', '''    static int s_xboxTransitionStage = -1;
    static u32 s_xboxTransitionDiagnosticFrames = 0;
    static int s_xboxTransitionRoom = -1;
    static int s_xboxTransitionLayer = -1;
    static fpc_ProcID s_xboxTransitionScene = fpcM_ERROR_PROCESS_ID_e;''', "transition identity storage")
source = once(source, '''    const bool gameplayReady = transitionStage >= 0 && transitionStage != Title_Screen &&
        dComIfGp_getPlayer(0) != nullptr && !mDoRst::isReset();
    if (gameplayReady && transitionStage == s_xboxTransitionStage) {''', '''    const int transitionRoom = dComIfGp_roomControl_getStayNo();
    const int transitionLayer = dComIfGp_getStartStageLayer();
    base_process_class* liveScene = fpcM_SearchByName(fpcNm_PLAY_SCENE_e);
    if (liveScene == nullptr) {
        liveScene = fpcM_SearchByName(fpcNm_OPENING_SCENE_e);
    }
    const fpc_ProcID transitionScene =
        liveScene != nullptr ? liveScene->id : fpcM_ERROR_PROCESS_ID_e;
    const bool diagnosticReady = transitionStage >= 0 && transitionStage != Title_Screen &&
        !mDoRst::isReset() && !tpr_xbox_reset_to_launcher_pending();
    const bool gameplayReady = transitionStage >= 0 && transitionStage != Title_Screen &&
        !mDoRst::isReset() && !tpr_xbox_reset_to_launcher_pending() &&
        tpr_xbox_scene_change_present() == 0 && tpr_xbox_create_queue_size() == 0 &&
        tpr_xbox_delete_queue_size() == 0 && liveScene != nullptr &&
        dComIfGp_getPlayer(0) != nullptr &&
        dComIfGp_getCamera(dComIfGp_getPlayerCameraID(0)) != nullptr &&
        !dComIfGp_event_runCheck() && !dComIfGp_isEnableNextStage();
    const bool sameScene = transitionStage == s_xboxTransitionStage &&
        transitionRoom == s_xboxTransitionRoom && transitionLayer == s_xboxTransitionLayer &&
        transitionScene == s_xboxTransitionScene;
    if (!sameScene) {
        s_xboxTransitionDiagnosticFrames = 0;
    }
    if (!gameplayReady || !sameScene) {
        s_xboxBlackTransitionFrames = 0;
        s_xboxTransitionRecoveryLatched = false;
        s_xboxOverlapCleanupPending = false;
        s_xboxOverlapCleanupAttempts = 0;
    }
    if (gameplayReady && sameScene) {''', "stable destination recovery gate")
source = once(source, '''        s_xboxTransitionStage = transitionStage;
        s_xboxTransitionReadyFrames = gameplayReady ? 1 : 0;''', '''        s_xboxTransitionStage = transitionStage;
        s_xboxTransitionRoom = transitionRoom;
        s_xboxTransitionLayer = transitionLayer;
        s_xboxTransitionScene = transitionScene;
        s_xboxTransitionReadyFrames = gameplayReady ? 1 : 0;''', "destination identity update")
source = once(source, '''    const bool opaqueBlack = globalFadeOpaque || jutFaderOpaque;
    if (opaqueBlack && gameplayReady) {''', '''    const bool opaqueBlack = globalFadeOpaque || jutFaderOpaque;
    // Reporting must remain active while creation/scene transactions are stalled.
    // These frames never make the destination eligible for destructive recovery.
    if (opaqueBlack && diagnosticReady) {
        if (s_xboxTransitionDiagnosticFrames < 1200) {
            ++s_xboxTransitionDiagnosticFrames;
        }
        if (!gameplayReady && s_xboxTransitionDiagnosticFrames >= 300 &&
            !s_xboxTransitionFailurePending)
        {
            captureXboxTransitionFailure(
                fader, transitionStage, s_xboxTransitionDiagnosticFrames,
                s_xboxTransitionReadyFrames);
        }
    } else {
        s_xboxTransitionDiagnosticFrames = 0;
    }
    if (opaqueBlack && gameplayReady) {''', "stall reporting independent of recovery")
source = once(source, '''        if (cleanupResult != 0 || overlapGone) {''', '''        if (cleanupResult != 0) {''', "recovery completion proof")
source = once(source, '''        const int cleanupResult = clearXboxTransitionFades();
        const bool overlapGone = !fopOvlpM_IsPeek() && !fopOvlpM_IsDoingReq();''', '''        const int cleanupResult = clearXboxTransitionFades();''', "recovery unused overlap result")
source = source.replace("build=1.4.1.729", "build=1.4.1.733")
write(path, source)

for path in ("src/dusk/ui/prelaunch.cpp",):
    source = read(path).replace("1.4.1.732", "1.4.1.733").replace("build=1.4.1.730", "build=1.4.1.733")
    write(path, source)

for relative, source in PENDING.items():
    (ROOT / relative).write_text(source, encoding="utf-8", newline="\n")

print("Applied .733 deferred reset completion, nonblocking reset audio/DSP progress, transaction-safe recovery and extended scene crash tracing.")
