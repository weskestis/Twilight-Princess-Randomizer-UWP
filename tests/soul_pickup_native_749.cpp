#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mods/svc/item.h>
#include <mods/items.h>
using u8 = uint8_t; using u16 = uint16_t; using u32 = uint32_t;
using s16 = int16_t; using f32 = float; using BOOL = int;
constexpr BOOL TRUE = 1, FALSE = 0;
constexpr u32 fpcM_ERROR_PROCESS_ID_e = UINT32_MAX;
#include "item_ids.h"
#include "enemy_soul_catalog.hpp"
#include "pickup_message_guard.hpp"
/* MESSAGE_CONSTANTS */

struct ModContext {};
struct fopAc_ac_c { u32 id = 0, mItemGiveTag = 0; };
struct daItemBase_c : fopAc_ac_c {
    u8 m_itemNo = 0xE0;
    int deaths = 0;
    u8 getItemNo() const { return m_itemNo; }
    void show() {} void hide() {} void dead() { ++deaths; }
};
struct daPy_frameCtrl_c {
    float frame = 30, end = 30;
    float getFrame() const { return frame; } float getEnd() const { return end; }
};
struct daAlink_c : fopAc_ac_c {
    enum { PROC_GET_ITEM = 0x148, ERFLG1_UNK_8, ANM_GET_A_WAIT, WANM_GET, ANM_HORSE_WAIT };
    int mProcID = PROC_GET_ITEM;
    int field_0x3198 = 0, field_0x2f99 = 12, field_0x2f94 = 0, mAlinkStaffId = 1, mEquipItem = 0;
    u32 field_0x32cc = 0, mMsgClassID = fpcM_ERROR_PROCESS_ID_e;
    float field_0x3478 = 9, field_0x347c = 9, field_0x3480 = 16, field_0x3484 = 1.0f / 7;
    struct { int field_0x3008 = 0; } mProcVar0;
    struct { int field_0x300a = 1; } mProcVar1;
    struct { s16 field_0x300c = 0xE0; } mProcVar2;
    struct { int field_0x300e = 0; } mProcVar3;
    struct { int field_0x3010 = 0; } mProcVar4;
    struct Angle { int y = 0; } shape_angle;
    struct { Angle angle; struct { float x = 0, z = 0; } pos; } current;
    struct { void setData(daItemBase_c*) {} } field_0x280c;
    struct { int getParam1() const { return 0; } } mDemo;
    struct Hio {
        struct { struct { struct { float mNormalInterpolation = 0; } m; } mWlMove; } mWolf;
        struct { struct { float mBasicInterpolation = 0; } m; } mBasic;
    } hio;
    Hio* mpHIO = &hio;
    daPy_frameCtrl_c mUnderFrameCtrl[1];
    bool checkModeFlg(int) const { return false; } bool checkReinRide() const { return false; }
    bool checkUnderMove0BckNoArc(int) const { return false; } bool checkWolf() const { return false; }
    bool checkAnmEnd(daPy_frameCtrl_c*) const { return false; }
    bool checkStageName(const char*) const { return false; } bool checkTreasureRupeeReturn(int) const { return false; }
    void onEndResetFlg1(int) {} void setSyncRidePos() {} void setGetItemFace(int) {} void setGetSubBgm(int) {}
    void setSingleAnimeWolfBase(int) {} void setSingleAnimeBase(int) {} void setBlendWolfMoveAnime(float) {}
    void setDoubleAnime(float, float, float, int, int, int, float) {} void setBlendMoveAnime(float) {}
    BOOL checkEndMessage(u32); int procCoGetItem();
};
constexpr int dItemNo_KANTERA_e = 0x48;
constexpr u8 dItemNo_NONE_e = 0xFF;
enum { Diababa, Fyrus, Morpheel, Stallord, Blizzeta, Armogohma, Argorok, Zant_Main_Room };
float cM_ssin(int) { return 0; } float cM_scos(int) { return 1; }
int cutEnds = 0, normalPoes = 0;
void dComIfGp_evmng_cutEnd(int) { ++cutEnds; }
int dComIfGs_getPohSpiritNum() { return normalPoes; }
int dComIfGs_getMaxLife() { return 12; }
bool dComIfGs_isItemFirstBit(int) { return false; }
daItemBase_c* partner = nullptr;
fopAc_ac_c* fopAcM_getItemEventPartner(daAlink_c*) { return partner; }
u32 fpcM_GetID(const fopAc_ac_c* actor) { return actor->id; }
u32 fopAcM_GetID(const fopAc_ac_c* actor) { return actor->id; }
bool partnerCreating = false;
bool fpcM_IsCreating(u32 id) { return partnerCreating && partner && id == partner->id; }
namespace randomizer::enemy_souls {
std::unordered_map<u32, u16> bindings;
std::optional<u16> logical_item_for_actor(const fopAc_ac_c* actor) {
    if (!actor) return std::nullopt;
    const auto it = bindings.find(actor->id);
    return it == bindings.end() ? std::nullopt : std::optional(it->second);
}
/* PRODUCTION_PHYSICAL_ITEM */
}
std::unordered_map<u32, std::string> names;
std::unordered_map<std::string, u16> rewards;
const char* checkName(ModContext*, u32 tag) {
    const auto it = names.find(tag); return it == names.end() ? nullptr : it->second.c_str();
}
struct UiToastDesc { const char* title_rml = nullptr; const char* body_rml = nullptr; int duration_ms = 0; };
#define UI_TOAST_DESC_INIT {}
struct Ui { void push_toast(void*, UiToastDesc*) {} };
namespace mods::log { template<class... T> void warn(T&&...) {} }
namespace randomizer::breakables {
std::unordered_set<u32> bound;
bool has_spawned_item_binding(const fopAc_ac_c* actor) { return bound.contains(actor->id); }
}
namespace randomizer::session {
struct { const ItemService* item = nullptr; ModContext* mod_ctx = nullptr; Ui* ui = nullptr; } svc_mng;
std::optional<u16> logicalItemForCheck(const char* name) {
    if (!name) return std::nullopt;
    const auto it = rewards.find(name); return it == rewards.end() ? std::nullopt : std::optional(it->second);
}
}
/* PRODUCTION_BOSS_SOUL_PREDICATE */
enum HookAction { HOOK_CONTINUE, HOOK_SKIP_ORIGINAL };
namespace mods { template<class T> T arg(void* args, int) { return static_cast<T>(args); } }

namespace randomizer::messages {
using MessageId = u16;
std::unordered_map<u32, MessageId> s_soulPickupMessages;
/* PRODUCTION_MESSAGE_RESOLVER */
}
/* PRODUCTION_ITEM_MESSAGE_FUNCTIONS */
namespace randomizer {
/* PRODUCTION_PICKUP_HOOK */
/* PRODUCTION_NATIVE_BRIDGES */
}
using randomizer::tpr_resolve_soul_pickup_message;
/* EXTRA_NATIVE_BRIDGE_USING */
enum { fopMsg_MODE_MSG_PREPARING_e = 1, fopMsg_MODE_BOX_OPENING_e = 2,
       fopMsg_MODE_MSG_DISPLAYED_e = 14, fopMsg_MODE_MSG_END_e = 16,
       fopMsg_MODE_BOX_CLOSED_e = 18, fopMsg_MODE_MSG_DESTROYED_e = 19 };
struct msg_class { int mode = fopMsg_MODE_MSG_PREPARING_e; u32 msg_idx = 0; } message;
u32 requestedMessage = 0;
bool messageAvailable = true;
int createdMessageMode = fopMsg_MODE_BOX_OPENING_e;
u32 fopMsgM_messageSet(u32 id, u32) {
    requestedMessage = id; message.msg_idx = id; message.mode = createdMessageMode;
    return messageAvailable ? 700 : 0;
}
msg_class* fopMsgM_SearchByID(u32 id) { return messageAvailable && id == 700 ? &message : nullptr; }
namespace randomizer {
PickupMessageGuard s_pickupMessageGuard;
PickupMessageGuard::Clock::time_point fixtureNow;
/* PRODUCTION_RECOVERY */
}
using randomizer::tpr_complete_breakable_pickup_message;
/* PRODUCTION_NATIVE_PICKUP */

int main() {
    using namespace randomizer;
    ItemService service{}; service.check_name = checkName; session::svc_mng.item = &service;
    for (u32 i = 0; i < BOSS_SOUL_MESSAGE_COUNT; ++i)
        messages::s_soulPickupMessages[BOSS_SOUL_MESSAGE_BASE + i] = 0x9000 + i;
    for (u32 i = 0; i < ENEMY_SOUL_MESSAGE_COUNT; ++i)
        messages::s_soulPickupMessages[ENEMY_SOUL_MESSAGE_BASE + i] = 0x9100 + i;
    const std::array<u8, 8> bosses{dItemNo_Randomizer_DIABABA_SOUL_e, dItemNo_Randomizer_FYRUS_SOUL_e,
        dItemNo_Randomizer_MORPHEEL_SOUL_e, dItemNo_Randomizer_STALLORD_SOUL_e,
        dItemNo_Randomizer_BLIZZETA_SOUL_e, dItemNo_Randomizer_ARMOGOHMA_SOUL_e,
        dItemNo_Randomizer_ARGOROK_SOUL_e, dItemNo_Randomizer_ZANT_SOUL_e};
    unsigned failures = 0, exercised = 0;
    for (int route = 0; route < 5; ++route) {
        for (u32 i = 0; i < ENEMY_SOUL_MESSAGE_COUNT + BOSS_SOUL_MESSAGE_COUNT; ++i) {
            const bool enemy = i < ENEMY_SOUL_MESSAGE_COUNT;
            const u32 index = enemy ? i : i - ENEMY_SOUL_MESSAGE_COUNT;
            const u16 logical = enemy ? enemy_souls::kFirstEnemySoulId + index : bosses[index];
            const u8 physical = enemy_souls::physical_item(logical);
            daItemBase_c item; item.id = 501; item.m_itemNo = physical;
            partner = &item; partnerCreating = false;
            enemy_souls::bindings.clear(); names.clear(); rewards.clear();
            if (enemy) enemy_souls::bindings[item.id] = logical;
            // Native check tags take precedence over an older actor preview.
            if (route == 3) {
                names[7] = "native_check"; rewards["native_check"] = logical; item.mItemGiveTag = 7;
                enemy_souls::bindings[item.id] = enemy_souls::kFirstEnemySoulId;
            }
            daAlink_c link; link.id = 500; link.mProcVar2.field_0x300c = route == 1 ? -1 : physical;
            requestedMessage = 0; cutEnds = 0; normalPoes = 19;
            // Route 0: normal pre-hook. Route 1: partner arrives after init; item byte was -1.
            // Route 2: direct optimized native call, without the procCoGetItem detour.
            if (route == 4) {
                // Actor creation completes after the first native pickup frame.
                partnerCreating = true;
                hookPreProcCoGetItem(nullptr, &link, nullptr, nullptr);
                link.procCoGetItem();
                assert(requestedMessage == 0 && item.deaths == 0);
                partnerCreating = false;
            } else if (route != 2) hookPreProcCoGetItem(nullptr, &link, nullptr, nullptr);
            link.procCoGetItem();
            const u32 expected = (enemy ? 0x9100 : 0x9000) + index;
            if (requestedMessage != expected) {
                ++failures;
                if (failures <= 6) std::printf("FAIL route=%d soul=%04x expected=%04x requested=%04x\n", route, logical, expected, requestedMessage);
            }
            assert(item.deaths == 0 && cutEnds == 0 && normalPoes == 19);
            // Native dialogue closure must still finish the event exactly once.
            message.mode = fopMsg_MODE_BOX_CLOSED_e;
            link.procCoGetItem();
            assert(item.deaths == 1 && cutEnds == 1 && link.field_0x3198 == 1);
            link.procCoGetItem();
            assert(item.deaths == 1);
            ++exercised;
        }
    }
    // Normal Poe counter thresholds keep their native wording; this fixture never grants a Poe.
    for (const int count : {0, 19, 59}) {
        daItemBase_c item; item.id = 501; partner = &item; enemy_souls::bindings.clear(); names.clear(); rewards.clear();
        daAlink_c link; link.id = 500; normalPoes = count;
        hookPreProcCoGetItem(nullptr, &link, nullptr, nullptr); link.procCoGetItem();
        assert(requestedMessage == 0x145 && normalPoes == count);
    }
    // Direct native ordinary items retain the base game's text selection.
    for (const u8 physical : {u8(0x01), u8(0x21), u8(0xE0)}) {
        daItemBase_c item; item.id = 501; item.m_itemNo = physical; partner = &item;
        enemy_souls::bindings.clear(); names.clear(); rewards.clear(); normalPoes = 19;
        daAlink_c link; link.mProcVar2.field_0x300c = physical;
        link.procCoGetItem();
        const u32 expected = physical == 0x21 ? 0x9D : physical == 0xE0 ? 0x4CF : 0x66;
        assert(requestedMessage == expected);
    }
    // A chosen, already opened message remains attached to its native actor.
    {
        daItemBase_c item; item.id = 501; partner = &item;
        enemy_souls::bindings[item.id] = enemy_souls::kFirstEnemySoulId;
        daAlink_c link; link.field_0x32cc = 0x1234; link.mMsgClassID = 700;
        message.mode = fopMsg_MODE_BOX_OPENING_e; requestedMessage = 0;
        link.procCoGetItem();
        assert(link.field_0x32cc == 0x1234 && requestedMessage == 0 && item.deaths == 0);
    }
    // An unavailable, preparing or destroyed Soul textbox uses the actual
    // recovery clock and then the whole native event/animation cleanup path.
    unsigned recovered = 0;
    for (const int unavailableMode : std::array<int, 3>{0, fopMsg_MODE_MSG_PREPARING_e, fopMsg_MODE_MSG_DESTROYED_e}) {
        for (u32 i = 0; i < ENEMY_SOUL_MESSAGE_COUNT + BOSS_SOUL_MESSAGE_COUNT; ++i) {
            const bool enemy = i < ENEMY_SOUL_MESSAGE_COUNT;
            const u32 index = enemy ? i : i - ENEMY_SOUL_MESSAGE_COUNT;
            const u16 logical = enemy ? enemy_souls::kFirstEnemySoulId + index : bosses[index];
            daItemBase_c item; item.id = 501; item.m_itemNo = enemy_souls::physical_item(logical);
            partner = &item; enemy_souls::bindings.clear(); names.clear(); rewards.clear();
            if (enemy) enemy_souls::bindings[item.id] = logical;
            daAlink_c link; link.id = 500; link.mProcVar2.field_0x300c = -1;
            s_pickupMessageGuard.reset(); requestedMessage = 0; cutEnds = 0; normalPoes = 19;
            messageAvailable = unavailableMode != 0; createdMessageMode = unavailableMode;
            for (int tick = 0; tick <= 500; ++tick) {
                fixtureNow = PickupMessageGuard::Clock::time_point{} + std::chrono::milliseconds(tick * 10);
                link.procCoGetItem();
                if (tick < 500) assert(item.deaths == 0 && cutEnds == 0);
            }
            assert(item.deaths == 1 && cutEnds == 1 && link.field_0x3198 == 1 && normalPoes == 19);
            if (enemy) assert(enemy_souls::bindings.at(item.id) == logical);
            link.procCoGetItem(); assert(item.deaths == 1);
            ++recovered;
        }
    }
    messageAvailable = true; createdMessageMode = fopMsg_MODE_BOX_OPENING_e;
    // Turning the randomizer off retires the registry. Static bridges must then
    // leave vanilla play alone, even if an obsolete tag is still present.
    messages::s_soulPickupMessages.clear(); session::svc_mng.item = nullptr;
    {
        daItemBase_c item; item.id = 501; item.mItemGiveTag = 7; partner = &item;
        enemy_souls::bindings[item.id] = enemy_souls::kFirstEnemySoulId;
        daAlink_c link; normalPoes = 19; link.procCoGetItem();
        assert(requestedMessage == 0x4CF);
    }
    std::printf("Exercised %u native Soul pickup routes; wrong message identity in %u\n", exercised, failures);
    std::printf("Recovered %u native Soul pickups after exactly five seconds with reward identity retained\n", recovered);
    return failures == 279 ? 42 : failures ? 43 : 0;
}
