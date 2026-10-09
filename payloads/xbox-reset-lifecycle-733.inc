#if defined(_UWP)
namespace {
bool s_xboxResetToLauncherPending = false;
}

extern "C" int tpr_xbox_scene_change_present() noexcept;
extern "C" int tpr_xbox_create_queue_size() noexcept;
extern "C" int tpr_xbox_delete_queue_size() noexcept;

bool tpr_xbox_reset_to_launcher_pending() noexcept {
    return s_xboxResetToLauncherPending;
}

// Called after aurora_end_frame, outside process iteration and hook callbacks.
// Keep the seed, save name, message tables and hooks until the old scene is gone.
void tpr_xbox_complete_reset_to_launcher() {
    if (!s_xboxResetToLauncherPending) {
        return;
    }
    if (mDoRst::isReset()) {
        dusk::startup_guard::stage("reset.wait-opening");
        return;
    }
    if (mDoAud_zelAudio_c::isResetFlag()) {
        dusk::startup_guard::stage("reset.wait-audio");
        return;
    }
    if (tpr_xbox_scene_change_present() != 0 ||
        tpr_xbox_create_queue_size() != 0 || tpr_xbox_delete_queue_size() != 0 ||
        fopOvlpM_IsPeek() || fopOvlpM_IsDoingReq())
    {
        dusk::startup_guard::stage("reset.wait-queues");
        return;
    }
    if (fpcM_SearchByName(fpcNm_OPENING_SCENE_e) == nullptr ||
        fpcM_SearchByName(fpcNm_PLAY_SCENE_e) != nullptr)
    {
        dusk::startup_guard::stage("reset.wait-opening-scene");
        return;
    }
    if (!mDoMemCd_isCardCommNone()) {
        dusk::startup_guard::stage("reset.wait-save");
        return;
    }

    dusk::startup_guard::begin_activity("reset.deactivate-game-mode");
    if (!dusk::gamemode::getGameModeManager().setCurrentGameMode(
            dusk::gamemode::kVanillaGameModeId))
    {
        dusk::startup_guard::stage("reset.deactivation-failed");
        return;
    }
    dusk::startup_guard::end_activity("reset.game-mode-deactivated");
    mDoRst::off3ButtonReset();
    mDoRst::offResetPrepare();
    JUTGamePad::C3ButtonReset::sResetOccurred = false;
    JUTGamePad::C3ButtonReset::sResetSwitchPushing = false;
    JUTGamePad::setResetCallback(mDoRst_resetCallBack, nullptr);
    dusk::IsGameLaunched = false;
    dusk::ui::return_to_prelaunch();
    dusk::ui::prelaunch_state().returnToPrelaunchOnReset = false;
    s_xboxResetToLauncherPending = false;

    // A reset crash must retain the active-session marker until the launcher exists.
    std::error_code ec;
    std::filesystem::remove(dusk::ConfigPath / "xbox-session-active.txt", ec);
    dusk::startup_guard::end_activity("reset.launcher-ready");
}
#endif
