namespace {
struct PureTrackerSlot {
    const char* label;
    const char* logicItem;
    u8 iconItem;
    int maximum;
};

constexpr std::array<PureTrackerSlot, 36> kPureTrackerSlots{{
    {"Progressive Fishing Rod", "Progressive Fishing Rod", dItemNo_Randomizer_FISHING_ROD_1_e, 2},
    {"Slingshot", "Slingshot", dItemNo_Randomizer_PACHINKO_e, 1},
    {"Lantern", "Lantern", dItemNo_Randomizer_KANTERA_e, 1},
    {"Gale Boomerang", "Gale Boomerang", dItemNo_Randomizer_BOOMERANG_e, 1},
    {"Iron Boots", "Iron Boots", dItemNo_Randomizer_HVY_BOOTS_e, 1},
    {"Progressive Bow", "Progressive Bow", dItemNo_Randomizer_BOW_e, 3},
    {"Bomb Bag", "Bomb Bag", dItemNo_Randomizer_BOMB_BAG_LV1_e, 3},
    {"Hawkeye", "Hawkeye", dItemNo_Randomizer_HAWK_EYE_e, 1},
    {"Progressive Clawshot", "Progressive Clawshot", dItemNo_Randomizer_W_HOOKSHOT_e, 2},
    {"Spinner", "Spinner", dItemNo_Randomizer_SPINNER_e, 1},
    {"Ball and Chain", "Ball and Chain", dItemNo_Randomizer_IRONBALL_e, 1},
    {"Progressive Dominion Rod", "Progressive Dominion Rod", dItemNo_Randomizer_COPY_ROD_e, 2},
    {"Empty Bottle", "Empty Bottle", dItemNo_Randomizer_EMPTY_BOTTLE_e, 4},
    {"Renados Letter", "Renados Letter", dItemNo_Randomizer_LETTER_e, 1},
    {"Invoice", "Invoice", dItemNo_Randomizer_BILL_e, 1},
    {"Wooden Statue", "Wooden Statue", dItemNo_Randomizer_WOOD_STATUE_e, 1},
    {"Ilias Charm", "Ilias Charm", dItemNo_Randomizer_IRIAS_PENDANT_e, 1},
    {"Horse Call", "Horse Call", dItemNo_Randomizer_HORSE_FLUTE_e, 1},
    {"Aurus Memo", "Aurus Memo", dItemNo_Randomizer_RAFRELS_MEMO_e, 1},
    {"Asheis Sketch", "Asheis Sketch", dItemNo_Randomizer_ASHS_SCRIBBLING_e, 1},
    {"Shadow Crystal", "Shadow Crystal", dItemNo_Randomizer_MAGIC_LV1_e, 1},
    {"Progressive Sword", "Progressive Sword", dItemNo_Randomizer_MASTER_SWORD_e, 4},
    {"Ordon Shield", "Ordon Shield", dItemNo_Randomizer_WOOD_SHIELD_e, 1},
    {"Progressive Wallet", "Progressive Wallet", dItemNo_Randomizer_WALLET_LV2_e, 2},
    {"Progressive Sky Book", "Progressive Sky Book", dItemNo_Randomizer_ANCIENT_DOCUMENT_e, 7},
    {"Progressive Hidden Skill", "Progressive Hidden Skill", dItemNo_Randomizer_ENDING_BLOW_e, 7},
    {"Poe Soul", "Poe Soul", dItemNo_Randomizer_POU_SPIRIT_e, 60},
    {"Zora Armor", "Zora Armor", dItemNo_Randomizer_WEAR_ZORA_e, 1},
    {"Magic Armor", "Magic Armor", dItemNo_Randomizer_ARMOR_e, 1},
    {"Hylian Shield", "Hylian Shield", dItemNo_Randomizer_HYLIA_SHIELD_e, 1},
    {"Progressive Fused Shadow", "Progressive Fused Shadow", dItemNo_Randomizer_FUSED_SHADOW_1_e, 3},
    {"Progressive Mirror Shard", "Progressive Mirror Shard", dItemNo_Randomizer_MIRROR_PIECE_2_e, 4},
    {"Gate Keys", "Gate Keys", dItemNo_Randomizer_BOSSRIDER_KEY_e, 1},
    {"Ordon Pumpkin", "Ordon Pumpkin", dItemNo_Randomizer_TOMATO_PUREE_e, 1},
    {"Ordon Cheese", "Ordon Cheese", dItemNo_Randomizer_TASTE_e, 1},
    {"North Faron Woods Gate Key", "North Faron Woods Gate Key", dItemNo_Randomizer_SMALL_KEY2_e, 1},
}};

std::unique_ptr<::randomizer::Randomizer> s_pureTrackerRando;
::randomizer::logic::item_pool::ItemPool s_pureTrackerInventory{};
std::string s_pureTrackerHash{};
bool s_pureTrackerReady = false;

bool s_xboxTransitionFailurePending = false;
std::string s_xboxTransitionFailureReport{};
u32 s_xboxTransitionFailureSerial = 0;

void clearXboxTransitionFades() noexcept {
    mDoGph_gInf_c::offFade();
    mDoGph_gInf_c::setFadeRate(0.0f);
    mDoGph_gInf_c::getFadeColor().a = 0;

    if (JUTFader* fader = mDoGph_gInf_c::getFader(); fader != nullptr) {
        fader->setStatus(JUTFader::None, 0);
        fader->setStatus(JUTFader::None, -1);
        fader->startFadeIn(1);
    }
}

void captureXboxTransitionFailure(
    JUTFader* fader, int stage, u32 blackFrames, u32 readyFrames)
{
    auto& context = randomizer_GetContext();
    std::ostringstream report;
    report << "Twilight Princess Randomizer Xbox transition failure report\n";
    report << "build=1.4.1.717\n";
    report << "failure_serial=" << ++s_xboxTransitionFailureSerial << "\n";
    report << "seed_hash=" << context.mHash << "\n";
    report << "stage_id=" << stage << "\n";
    report << "room=" << static_cast<int>(dComIfGp_roomControl_getStayNo()) << "\n";
    report << "player_present=" << (dComIfGp_getPlayer(0) != nullptr ? 1 : 0) << "\n";
    report << "event_running=" << (dComIfGp_event_runCheck() ? 1 : 0) << "\n";
    report << "next_stage_enabled=" << (dComIfGp_isEnableNextStage() ? 1 : 0) << "\n";
    report << "overlap_peek=" << (fopOvlpM_IsPeek() ? 1 : 0) << "\n";
    report << "black_frames=" << blackFrames << "\n";
    report << "stage_ready_frames=" << readyFrames << "\n";
    report << "global_fade_active=" << static_cast<int>(mDoGph_gInf_c::isFade()) << "\n";
    report << "global_fade_rate=" << mDoGph_gInf_c::getFadeRate() << "\n";
    report << "global_fade_speed=" << mDoGph_gInf_c::getFadeSpeed() << "\n";
    report << "global_fade_alpha=" << static_cast<int>(mDoGph_gInf_c::getFadeColor().a) << "\n";
    if (fader != nullptr) {
        report << "jutfader_status=" << fader->getStatus() << "\n";
        report << "jutfader_timer=" << fader->mTimer << "\n";
        report << "jutfader_duration=" << fader->mDuration << "\n";
        report << "jutfader_status_timer=" << fader->mStatusTimer << "\n";
        report << "jutfader_next_status=" << fader->mNextStatus << "\n";
    } else {
        report << "jutfader=null\n";
    }

    s_xboxTransitionFailureReport = report.str();
    s_xboxTransitionFailurePending = true;
}

constexpr uint32_t kPureTrackerStageStableFrames = 300;
uint32_t s_pureTrackerStageStableFrames = 0;
int s_pureTrackerObservedStage = -1;

void resetPureTrackerStageGate() noexcept {
    s_pureTrackerStageStableFrames = 0;
    s_pureTrackerObservedStage = -1;
}

bool pureTrackerRuntimeReady() noexcept {
    auto& context = randomizer_GetContext();
    if (!g_seedActivated || context.mCreatingSave || context.mHash.empty() ||
        !randomizer_IsActive() || !g_randomizerState.mInitialized ||
        g_randomizerState.getRoomReloadingState() ||
        dComIfGp_getPlayer(0) == nullptr || dComIfGp_event_runCheck())
    {
        resetPureTrackerStageGate();
        return false;
    }

    const int stage = getStageID();
    if (stage < 0 || stage == Title_Screen) {
        resetPureTrackerStageGate();
        return false;
    }

    if (s_pureTrackerObservedStage != stage) {
        s_pureTrackerObservedStage = stage;
        s_pureTrackerStageStableFrames = 0;
        return false;
    }

    if (s_pureTrackerStageStableFrames < kPureTrackerStageStableFrames) {
        ++s_pureTrackerStageStableFrames;
        return false;
    }

    return true;
}

bool refreshPureTrackerSnapshot() {
    if (!pureTrackerRuntimeReady()) {
        return false;
    }
    auto& context = randomizer_GetContext();

    const bool needsWorldBuild =
        !s_pureTrackerRando || s_pureTrackerHash != context.mHash;
    const bool needsInventoryRefresh =
        needsWorldBuild || g_randomizerState.mUpdateTracker;
    if (!needsInventoryRefresh) {
        return s_pureTrackerReady;
    }

    g_randomizerState.mUpdateTracker = false;

    try {
        if (needsWorldBuild) {
            s_pureTrackerHash = context.mHash;
            s_pureTrackerReady = false;
            s_pureTrackerInventory.clear();
            s_pureTrackerRando =
                std::make_unique<::randomizer::Randomizer>(::randomizer::paths::GetRandomizerPath());
            s_pureTrackerRando->GenerateTrackerWorld();
        }

        auto* world = s_pureTrackerRando ? s_pureTrackerRando->GetWorld() : nullptr;
        if (world == nullptr) {
            mods::log::warn("Pure Item Tracker could not build a tracker world for {}", context.mHash);
            s_pureTrackerRando.reset();
            return false;
        }

        // Inventory changes do not require rebuilding the logic world. Re-snapshot only
        // the current save state so item pickups cannot trigger a full generator pass.
        s_pureTrackerInventory = getSaveItemPool(world);
        s_pureTrackerReady = true;
    } catch (const std::exception& error) {
        mods::log::warn("Pure Item Tracker refresh failed for {}: {}", context.mHash, error.what());
        s_pureTrackerRando.reset();
        s_pureTrackerReady = false;
    } catch (...) {
        mods::log::warn("Pure Item Tracker refresh failed for {}: unknown error", context.mHash);
        s_pureTrackerRando.reset();
        s_pureTrackerReady = false;
    }
    return s_pureTrackerReady;
}
}  // namespace

extern "C" bool tpr_xbox_transition_failure_pending() noexcept {
    return s_xboxTransitionFailurePending;
}

extern "C" const char* tpr_xbox_transition_failure_report() noexcept {
    return s_xboxTransitionFailureReport.c_str();
}

extern "C" void tpr_xbox_transition_failure_acknowledge() noexcept {
    s_xboxTransitionFailurePending = false;
}

extern "C" void tpr_xbox_transition_retry_recovery() noexcept {
    clearXboxTransitionFades();
}

extern "C" bool tpr_pure_tracker_active() noexcept {
    return refreshPureTrackerSnapshot();
}

extern "C" std::size_t tpr_pure_tracker_slot_count() noexcept {
    return kPureTrackerSlots.size();
}

extern "C" const char* tpr_pure_tracker_slot_label(std::size_t index) noexcept {
    return index < kPureTrackerSlots.size() ? kPureTrackerSlots[index].label : "";
}

extern "C" uint8_t tpr_pure_tracker_slot_icon(std::size_t index) noexcept {
    return index < kPureTrackerSlots.size() ? kPureTrackerSlots[index].iconItem : 0;
}

extern "C" int tpr_pure_tracker_slot_max(std::size_t index) noexcept {
    return index < kPureTrackerSlots.size() ? kPureTrackerSlots[index].maximum : 0;
}

extern "C" int tpr_pure_tracker_slot_owned(std::size_t index) noexcept {
    if (index >= kPureTrackerSlots.size() || !refreshPureTrackerSnapshot() || !s_pureTrackerRando) {
        return 0;
    }
    auto* world = s_pureTrackerRando->GetWorld();
    if (world == nullptr) {
        return 0;
    }
    auto* item = world->GetItem(kPureTrackerSlots[index].logicItem, true);
    if (item == nullptr) {
        return 0;
    }
    const int count = static_cast<int>(
        std::count(s_pureTrackerInventory.begin(), s_pureTrackerInventory.end(), item));
    return std::clamp(count, 0, kPureTrackerSlots[index].maximum);
}
