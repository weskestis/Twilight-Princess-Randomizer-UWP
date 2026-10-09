"""Compile the transformed runtime functions against controlled lifecycle states.

These tests exercise production function bodies, not a second model of the fix.
The real Windows/UWP compile and 58 generator scenarios remain separate CI steps.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def function(source: str, signature: str) -> str:
    start = source.index(signature)
    token = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/|[{}]')
    depth = 0
    opened = False
    for match in token.finditer(source, start):
        if match.group() == "{":
            opened = True
            depth += 1
        elif match.group() == "}":
            depth -= 1
            if opened and depth == 0:
                return source[start:match.end()]
    raise RuntimeError(f"Unclosed test function: {signature}")


parser = argparse.ArgumentParser()
parser.add_argument("source", nargs="?", default=os.environ.get("TPR_SRC"))
args = parser.parse_args()
root = Path(args.source)
reset = (root / "src/m_Do/m_Do_Reset.cpp").read_text(encoding="utf-8")
session = (root / "mods/randomizer/src/session.cpp").read_text(encoding="utf-8")
audio = (root / "src/dusk/audio/DuskAudioSystem.cpp").read_text(encoding="utf-8")
z2 = (root / "src/Z2AudioLib/Z2AudioMgr.cpp").read_text(encoding="utf-8")
main = (root / "src/m_Do/m_Do_main.cpp").read_text(encoding="utf-8")

poll_start = main.index("tpr_xbox_complete_reset_to_launcher();", main.index("void main01(void)"))
assert main.rfind("aurora_end_frame();", 0, poll_start) > main.index("void main01(void)")
assert main.rfind("fapGm_Execute();", 0, poll_start) < main.rfind("aurora_end_frame();", 0, poll_start)
assert "if (cleanupResult != 0 || overlapGone)" not in session
assert "transitionRoom == s_xboxTransitionRoom" in session
assert "transitionScene == s_xboxTransitionScene" in session

payload_start = reset.index("#if defined(_UWP)\nnamespace {\nbool s_xboxResetToLauncherPending")
callback_start = reset.index("void mDoRst_resetCallBack(int port, void*)")
payload = reset[payload_start:callback_start]
callback = function(reset, "void mDoRst_resetCallBack(int port, void*)")
recovery = function(session, "int clearXboxTransitionFades() noexcept")
# Exercise the exact production early-return gate. No renderer/actor stubs can
# approximate the restoration itself; that part is exercised on Xbox.
recovery_gate = recovery[:recovery.index("    int cancelResult = 1;")]
recovery_gate += "    ++recoveryEffects;\n    return 1;\n}"
reset_pump = function(audio, "void dusk::audio::Pump()")
reset_pump = reset_pump[:reset_pump.index("#endif") + len("#endif")] + "\n}"
gframe = function(z2, "void Z2AudioMgr::gframeProcess()")
tick = function(session, "ModResult onGameModeUpdate(void*, ModError*)")

stubs = r'''
#define _UWP 1
#define TARGET_PC 1
#define PLATFORM_GCN 1
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
using u32 = uint32_t;
using fpc_ProcID = uint32_t;
struct ModError {};
using ModResult = int;
constexpr int MOD_OK = 0;
struct base_process_class {};
base_process_class opening, play;
constexpr int fpcNm_OPENING_SCENE_e = 1, fpcNm_PLAY_SCENE_e = 2;
int sceneQueue = 0, createQueue = 0, deleteQueue = 0;
bool peek = false, doing = false, openingExists = true, playExists = false;
bool cardIdle = true, playerExists = true, cameraExists = true;
bool eventRunning = false, nextStage = false;
int recalibrations = 0, resetCallbacks = 0, setCalls = 0;
int closeCalls = 0, launcherCalls = 0, uiTicks = 0, gameTicks = 0, recoveryEffects = 0;
bool recurseReset = false, deactivateSucceeds = true;
bool s_xboxTransitionFailurePending = false, s_xboxTransitionDeferredReported = false;
std::string s_xboxTransitionFailureReport;
void mDoRst_resetCallBack(int, void*);
struct mDoRst {
    inline static bool reset = false, three = false, prepare = false;
    inline static int resetPort = -1;
    static bool isReset() { return reset; }
    static bool is3ButtonReset() { return three; }
    static void onReset() { reset = true; }
    static void offReset() { reset = false; }
    static void on3ButtonReset() { three = true; }
    static void off3ButtonReset() { three = false; }
    static void offResetPrepare() { prepare = false; }
    static void set3ButtonResetPort(int port) { resetPort = port; }
};
struct mDoAud_zelAudio_c {
    inline static bool init = true, audioReset = false;
    static bool isInitFlag() { return init; }
    static bool isResetFlag() { return audioReset; }
};
struct JUTGamePad {
    struct C3ButtonReset {
        inline static bool sResetOccurred = false, sResetSwitchPushing = false;
    };
    static void setResetCallback(void (*)(int, void*), void*) {}
};
void cAPICPad_recalibrate() { ++recalibrations; }
bool mDoMemCd_isCardCommNone() { return cardIdle; }
bool fopOvlpM_IsPeek() { return peek; }
bool fopOvlpM_IsDoingReq() { return doing; }
base_process_class* fpcM_SearchByName(int name) {
    if (name == fpcNm_OPENING_SCENE_e) return openingExists ? &opening : nullptr;
    return playExists ? &play : nullptr;
}
void* dComIfGp_getPlayer(int) { return playerExists ? &play : nullptr; }
int dComIfGp_getPlayerCameraID(int) { return 0; }
void* dComIfGp_getCamera(int) { return cameraExists ? &play : nullptr; }
bool dComIfGp_event_runCheck() { return eventRunning; }
bool dComIfGp_isEnableNextStage() { return nextStage; }
extern "C" int tpr_xbox_scene_change_present() noexcept { return sceneQueue; }
extern "C" int tpr_xbox_create_queue_size() noexcept { return createQueue; }
extern "C" int tpr_xbox_delete_queue_size() noexcept { return deleteQueue; }
namespace dusk {
bool IsGameLaunched = true;
std::filesystem::path ConfigPath;
namespace startup_guard {
std::string stageValue;
void stage(const char* s) { stageValue = s; }
void begin_activity(const char* s) { stage(s); }
void end_activity(const char* s) { stage(s); }
}
namespace ui {
struct PrelaunchState { bool returnToPrelaunchOnReset = false; } state;
PrelaunchState& prelaunch_state() { return state; }
void close_all_documents() { ++closeCalls; }
void return_to_prelaunch() { ++launcherCalls; }
}
namespace gamemode {
constexpr int kVanillaGameModeId = 0;
struct GameMode {
    void invokeOnGameResetFunction() const {
        ++resetCallbacks;
        assert(mDoRst::isReset());
        if (recurseReset) mDoRst_resetCallBack(-1, nullptr);
    }
} mode;
struct GameModeManager {
    const GameMode* getCurrentGameMode() { return &mode; }
    bool setCurrentGameMode(int) { ++setCalls; return deactivateSucceeds; }
} manager;
GameModeManager& getGameModeManager() { return manager; }
}
namespace audio { void Pump(); }
}
namespace ui { void update() { ++uiTicks; } }
namespace session { void update() { ++gameTicks; } }
struct JASCriticalSection { JASCriticalSection() {} ~JASCriticalSection() {} };
namespace JASDriver { u32 getSubFrames() { return 6; } }
struct JASAudioThread {
    inline static int snIntCount = 0;
    static void setDSPSyncCount(u32 n) { snIntCount = static_cast<int>(n); }
};
int OutChannelCount = 2, renders = 0, renderResult = 1;
int RenderAudioSubframe() { ++renders; return renderResult; }
struct Z2AudioMgr {
    bool mResettingFlag = false, field_0x519 = false;
    int gameplayCalls = 0, frameworkCalls = 0;
    struct SoundMgr {
        int stops = 0, syncStops = 0, frames = 0;
        void stop() { ++stops; }
        void stopSync() { ++syncStops; }
        void framework() { ++frames; }
    } mSoundMgr;
    struct Reseter { bool checkDone() { return true; } } mAudioReseter;
    void zeldaGFrameWork() { ++gameplayCalls; }
    void framework() { ++frameworkCalls; }
    void gframeProcess();
};
'''

cases = r'''
int main(int argc, char** argv) {
    assert(argc == 2);
    dusk::ConfigPath = std::filesystem::path(argv[1]);
    const auto marker = dusk::ConfigPath / "xbox-session-active.txt";
    { std::ofstream file(marker); file << "active"; }
    recurseReset = true;
    mDoRst_resetCallBack(-1, nullptr);
    assert(tpr_xbox_reset_to_launcher_pending() && mDoRst::isReset());
    assert(resetCallbacks == 1 && closeCalls == 1 && setCalls == 0 && launcherCalls == 0);
    mDoRst_resetCallBack(-1, nullptr);
    assert(resetCallbacks == 1 && recalibrations == 1);
    onGameModeUpdate(nullptr, nullptr);
    assert(uiTicks == 0 && gameTicks == 0);
    auto blocked = [] {
        tpr_xbox_complete_reset_to_launcher();
        assert(tpr_xbox_reset_to_launcher_pending());
        assert(setCalls == 0 && launcherCalls == 0 && dusk::IsGameLaunched);
        assert(std::filesystem::exists(dusk::ConfigPath / "xbox-session-active.txt"));
    };
    blocked();
    mDoRst::offReset();
    mDoAud_zelAudio_c::audioReset = true; blocked(); mDoAud_zelAudio_c::audioReset = false;
    sceneQueue = 1; blocked(); sceneQueue = 0;
    createQueue = 1; blocked(); createQueue = 0;
    deleteQueue = 1; blocked(); deleteQueue = 0;
    peek = true; blocked(); peek = false;
    doing = true; blocked(); doing = false;
    openingExists = false; blocked(); openingExists = true;
    playExists = true; blocked(); playExists = false;
    cardIdle = false; blocked(); cardIdle = true;
    deactivateSucceeds = false;
    tpr_xbox_complete_reset_to_launcher();
    assert(tpr_xbox_reset_to_launcher_pending() && launcherCalls == 0);
    assert(std::filesystem::exists(marker));
    deactivateSucceeds = true;
    tpr_xbox_complete_reset_to_launcher();
    assert(!tpr_xbox_reset_to_launcher_pending() && !dusk::IsGameLaunched);
    assert(launcherCalls == 1 && setCalls == 2 && !std::filesystem::exists(marker));
    tpr_xbox_complete_reset_to_launcher();
    assert(launcherCalls == 1 && setCalls == 2);
    onGameModeUpdate(nullptr, nullptr);
    assert(uiTicks == 1 && gameTicks == 1);

    // A later reset is accepted, then a rejected three-button request does no teardown.
    dusk::IsGameLaunched = true;
    mDoRst_resetCallBack(0, nullptr);
    assert(resetCallbacks == 2 && mDoRst::resetPort == 0 && mDoRst::three);
    mDoRst::offReset();
    tpr_xbox_complete_reset_to_launcher();
    assert(launcherCalls == 2 && !mDoRst::three);
    mDoRst::three = true;
    mDoRst_resetCallBack(0, nullptr);
    assert(!tpr_xbox_reset_to_launcher_pending() && resetCallbacks == 2);
    mDoRst::three = false;

    // Production recovery must refuse to touch even an old, still-visible scene.
    playExists = true;
    assert(clearXboxTransitionFades() == 1);
    auto recoveryBlocked = [] {
        const int before = recoveryEffects;
        assert(clearXboxTransitionFades() == 0 && recoveryEffects == before);
    };
    sceneQueue = 1; recoveryBlocked(); sceneQueue = 0;
    createQueue = 1; recoveryBlocked(); createQueue = 0;
    deleteQueue = 1; recoveryBlocked(); deleteQueue = 0;
    eventRunning = true; recoveryBlocked(); eventRunning = false;
    nextStage = true; recoveryBlocked(); nextStage = false;
    playerExists = false; recoveryBlocked(); playerExists = true;
    cameraExists = false; recoveryBlocked(); cameraExists = true;
    playExists = false; openingExists = false; recoveryBlocked(); openingExists = true;
    mDoRst::onReset(); recoveryBlocked(); mDoRst::offReset();
    s_xboxResetToLauncherPending = true; recoveryBlocked(); s_xboxResetToLauncherPending = false;
    assert(clearXboxTransitionFades() == 1);

    // No output voice/queue is needed to advance reset DSP work. Audio rendering
    // is still refused unless the initialized engine actually owns a reset.
    mDoRst::onReset(); mDoAud_zelAudio_c::audioReset = true;
    dusk::audio::Pump();
    assert(renders == 6 && JASAudioThread::snIntCount == 0);
    renders = 0; OutChannelCount = 0; dusk::audio::Pump(); assert(renders == 0); OutChannelCount = 2;
    mDoAud_zelAudio_c::init = false; dusk::audio::Pump(); assert(renders == 0); mDoAud_zelAudio_c::init = true;
    mDoAud_zelAudio_c::audioReset = false; dusk::audio::Pump(); assert(renders == 0); mDoAud_zelAudio_c::audioReset = true;
    renderResult = 0; dusk::audio::Pump(); assert(renders == 1 && JASAudioThread::snIntCount == 5);
    renders = 0; renderResult = 1; mDoRst::offReset(); dusk::audio::Pump(); assert(renders == 0);
    Z2AudioMgr sound;
    mDoRst::onReset(); sound.mResettingFlag = true; sound.gframeProcess();
    assert(sound.mSoundMgr.stops == 1 && sound.mSoundMgr.frames == 1 && sound.frameworkCalls == 1);
    assert(sound.gameplayCalls == 0 && sound.mSoundMgr.syncStops == 0);
    mDoRst::offReset(); sound.mResettingFlag = false; sound.gframeProcess();
    assert(sound.gameplayCalls == 1 && sound.mSoundMgr.frames == 2 && sound.frameworkCalls == 2);
    std::cout << "PASS .733 native lifecycle tests: duplicate/recursive reset, delayed teardown, busy save, scene queues, safe recovery, reset DSP and nonblocking sound drain.\n";
}
'''

with tempfile.TemporaryDirectory(prefix="tpr-733-tests-") as temp:
    work = Path(temp)
    cpp = work / "lifecycle.cpp"
    cpp.write_text("\n".join([stubs, payload, callback, recovery_gate, reset_pump, gframe, tick, cases]), encoding="utf-8")
    if os.name == "nt":
        compiler = shutil.which("cl")
        if compiler is None:
            raise RuntimeError("MSVC x64 compiler is required for the Windows lifecycle test")
        exe = work / "lifecycle.exe"
        command = [compiler, "/nologo", "/std:c++20", "/EHsc", "/W4", "/WX", str(cpp), f"/Fe:{exe}"]
    else:
        compiler = shutil.which("g++")
        if compiler is None:
            raise RuntimeError("g++ is required for the local lifecycle test")
        exe = work / "lifecycle"
        command = [compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror", "-Wno-unused-variable", str(cpp), "-o", str(exe)]
    subprocess.run(command, cwd=work, check=True)
    subprocess.run([str(exe), str(work)], cwd=work, check=True)
