"""Compile production award, identity and persistence functions with a memory save service.

The generator's separate 62-scenario certification checks placement and reachability.
This harness checks late save notifications, exact Soul IDs, normal Poe isolation,
native check completion, pickup text, sidecar reloads, and actor/scene retirement.
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
        if match.group() == '{':
            opened = True
            depth += 1
        elif match.group() == '}':
            depth -= 1
            if opened and depth == 0:
                return source[start:match.end()]
    raise RuntimeError(f'Unclosed production function: {signature}')

parser = argparse.ArgumentParser()
parser.add_argument('source', nargs='?', default=os.environ.get('TPR_SRC'))
root = Path(parser.parse_args().source)
mod = root / 'mods/randomizer'
session = (mod / 'src/session.cpp').read_text(encoding='utf-8')
breakables = (mod / 'src/breakables.cpp').read_text(encoding='utf-8')
souls = (mod / 'src/enemy_souls.cpp').read_text(encoding='utf-8')
hooks = (mod / 'src/hooks.cpp').read_text(encoding='utf-8')
tools = (mod / 'src/tools.cpp').read_text(encoding='utf-8')
item = (mod / 'src/item.cpp').read_text(encoding='utf-8')
host_gives = (root / 'src/dusk/mods/item_gives.cpp').read_text(encoding='utf-8')
host_service = (root / 'src/dusk/mods/svc/item.cpp').read_text(encoding='utf-8')
context = (mod / 'src/randomizer_context.hpp').read_text(encoding='utf-8')

# Changes must preserve the three-field public resolver ABI and .733 lifecycle guards.
sdk = (root / 'sdk/include/mods/svc/item.h').read_text(encoding='utf-8')
assert '#define ITEM_SERVICE_MINOR 4u' in sdk
resolution = function(sdk, 'typedef struct ItemCheckResolution')
assert resolution.count(';') == 3
for name in ['mPoeOverrides', 'mBugRewardOverrides', 'mSkyCharacterOverrides', 'mGoldenWolfOverrides', 'mShopOverrides']:
    assert re.search(r'std::unordered_map<[^,>]+, u16> '+name, context), name
for name in ['breakableLocationData', 'expandedShopLocationData']:
    assert 'u16 itemId{0xFF};' in function(context, 'struct '+name), name
assert 's_actorKeys.clear()' not in function(breakables, 'void ensure_collection_locked()')
assert 's_tagKeys.clear()' not in function((mod / 'src/shop_items.cpp').read_text(), 'void ensure_collection_locked()')
assert 'noteSaveSlot(slot, true)' in function(session, 'void onObservedSaveLoaded(')
assert 'noteSaveSlot(slot, true)' in function(session, 'void onObservedNewSave(')
assert 'noteSaveSlot(slot, true)' not in function(session, 'void onSaveWritten(')
assert 'item::exec_item_get(*breakableItem)' in function(hooks, 'HookAction hookPreItemItemGet(')
assert 'completeLogicalCheck(actor, actor->m_itemNo)' in function(hooks, 'HookAction hookPreDemoItemActionEvent(')
assert 'ADD_HOOK_PRE(execItemGet_LogicalSoul, hookPreExecItemGetLogicalSoul)' in hooks
assert 'uninstall<execItemGet_LogicalSoul>' in hooks
assert 'mTreasureChestEnemySoulOverrides.find(key)' in function(tools, 'int getLocationItem(')
assert 'mFreestandingEnemySoulOverrides.find(key)' in function(tools, 'int getLocationItem(')

stubs = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mods/svc/item.h>
#include <mods/items.h>
using u8 = uint8_t; using u16 = uint16_t; using u32 = uint32_t;
using fpc_ProcID = uint32_t;
#include "src/item_ids.h"
#include "src/runtime_item_ids.hpp"
/*SOUL_MESSAGE_CONSTANTS*/
constexpr auto fpcM_ERROR_PROCESS_ID_e = UINT32_MAX;
constexpr u8 dItemNo_NONE_e = 0xFF;
class fopAc_ac_c {
public:
    u32 id = 0, mItemGiveTag = 0;
};
struct daItemBase_c : fopAc_ac_c { u8 m_itemNo = dItemNo_Randomizer_POU_SPIRIT_e; };
struct daAlink_c {
    u16 field_0x32cc = 0;
    struct { u8 field_0x300c = dItemNo_Randomizer_POU_SPIRIT_e; } mProcVar2;
};
daItemBase_c* partner = nullptr;
fopAc_ac_c* fopAcM_getItemEventPartner(daAlink_c*) { return partner; }
u32 fopAcM_GetID(const fopAc_ac_c* actor) { return actor->id; }
u32 fpcM_GetID(const fopAc_ac_c* actor) { return actor->id; }
bool fpcM_IsCreating(u32) { return false; }
int normalPoes = 0, vanillaAwards = 0, nativeCompletions = 0;
std::unordered_set<u8> firstItemBits;
int dComIfGs_getPohSpiritNum() { return normalPoes; }
int dComIfGs_getMaxLife() { return 12; }
void dComIfGs_onItemFirstBit(u8 id) { ++vanillaAwards; firstItemBits.insert(id); }
bool dComIfGs_isItemFirstBit(u8 id) { return firstItemBits.contains(id); }
u32 verifyProgressiveItem(u32 id) { return id; }
u8 randomizer_getRandomFoolishItemModelID(const char*) { return 1; }
bool randomizer_IsBossSoulItem(u8 id) {
    constexpr std::array ids{dItemNo_Randomizer_DIABABA_SOUL_e,
        dItemNo_Randomizer_FYRUS_SOUL_e, dItemNo_Randomizer_MORPHEEL_SOUL_e,
        dItemNo_Randomizer_STALLORD_SOUL_e, dItemNo_Randomizer_BLIZZETA_SOUL_e,
        dItemNo_Randomizer_ARMOGOHMA_SOUL_e, dItemNo_Randomizer_ARGOROK_SOUL_e,
        dItemNo_Randomizer_ZANT_SOUL_e};
    return std::ranges::find(ids, id) != ids.end();
}
int getStageID(const char* name) {
    if (std::strcmp(name,"F_SP103") == 0) return 43;
    if (std::strcmp(name,"D_MN05A") == 0) return 2;
    return -1;
}
constexpr int Ook = 2;
struct Context {
    std::string mHash = "test-seed";
    struct Named { int itemId; };
    struct Breakable { u16 itemId; };
    std::unordered_map<std::string,Named> mItemLocations;
    std::unordered_map<u16,u16> mTreasureChestEnemySoulOverrides, mFreestandingEnemySoulOverrides;
    std::unordered_map<u16,u8> mTreasureChestOverrides, mFreestandingItemOverrides;
    std::unordered_map<u16,u16> mPoeOverrides, mGoldenWolfOverrides, mSkyCharacterOverrides;
    std::unordered_map<u8,u16> mBugRewardOverrides;
    std::unordered_map<u32,u16> mShopOverrides;
    std::unordered_map<uint64_t,Breakable> mBreakableOverrides;
} testContext;
Context& randomizer_GetContext() { return testContext; }
struct { bool mUpdateTracker = false; } g_randomizerState;
namespace mods::log {
template<typename... Args> void error(const char*, const Args&...) {}
template<typename... Args> void warn(const char*, const Args&...) {}
}
enum HookAction { HOOK_CONTINUE, HOOK_SKIP_ORIGINAL };
struct HookArgs { u8 physical; u32 tag; fopAc_ac_c* giver; };
namespace mods {
template<typename T> T arg(void* args, int) {
    if constexpr (std::is_same_v<T,u8>) return static_cast<HookArgs*>(args)->physical;
    else if constexpr (std::is_same_v<T,u32>) return static_cast<HookArgs*>(args)->tag;
    else if constexpr (std::is_same_v<T,daAlink_c*>) return static_cast<T>(args);
    else return static_cast<HookArgs*>(args)->giver;
}
}
namespace randomizer::session {
std::mutex s_save_state_mutex;
std::int32_t s_current_save_slot = -1;
std::uint64_t s_save_state_revision = 1;
struct { const ItemService* item; ModContext* mod_ctx; } svc_mng{};
std::map<std::pair<int,std::string>,std::vector<u8>> saveBlobs;
std::optional<std::vector<u8>> getSaveBlob(std::string_view name) {
    auto found = saveBlobs.find({s_current_save_slot, std::string(name)});
    return found == saveBlobs.end() ? std::nullopt : std::optional(found->second);
}
bool setSaveBlob(std::string_view name, const void* data, size_t size) {
    const auto* bytes = static_cast<const u8*>(data);
    saveBlobs[{s_current_save_slot,std::string(name)}] = std::vector<u8>(bytes, bytes+size);
    return true;
}
}
'''

message_constants = '\n'.join(line for line in (mod / 'src/custom_flow_ids.hpp').read_text().splitlines()
                              if line.startswith('inline constexpr'))
parts = [stubs.replace('/*SOUL_MESSAGE_CONSTANTS*/', message_constants), 'namespace randomizer::session {']
for sig in ['void noteSaveSlot(', 'std::int32_t currentSaveSlot()', 'std::uint64_t saveStateRevision()']:
    parts.append(function(session, sig))
parts.append('}')
parts.append('namespace randomizer::breakables {')
start = breakables.index('constexpr std::string_view kCollectionBlobName')
end = breakables.index('uint64_t seed_fingerprint', start)
parts.append(breakables[start:end])
for sig in ['uint64_t seed_fingerprint(', 'void append_u16(', 'void append_u32(', 'void append_u64(',
            'std::optional<uint16_t> read_u16(', 'std::optional<uint32_t> read_u32(', 'std::optional<uint64_t> read_u64(',
            'void load_collection_locked(', 'void ensure_collection_locked()', 'void persist_collection_locked()',
            'std::optional<uint64_t> spawned_key_locked(', 'bool is_shuffled_and_uncollected(',
            'bool should_show_marker(', 'std::optional<uint16_t> spawned_item_assignment(',
            'void collect_spawned_item(', 'void forget_actor(', 'void forget_spawned_item(', 'void clear_scene_actors()']:
    parts.append(function(breakables, sig))
parts.append('}')
parts.append('namespace randomizer::enemy_souls {')
parts.append('void associate_item(fpc_ProcID, std::uint16_t, std::optional<std::uint16_t> = std::nullopt) noexcept;')
start = souls.index('constexpr std::string_view kStateBlobName')
parts.append(souls[start:souls.index('enum class PermissionKind', start)])
parts.append(function(souls, 'struct ItemAssociation')+';')
parts.append(function(souls, 'struct BlockedActorState')+';')
start = souls.index('std::mutex s_mutex;')
parts.append(souls[start:souls.index('constexpr std::uint16_t soul(', start)])
for sig in ['std::uint64_t seed_fingerprint(', 'void append_u16(', 'void append_u32(', 'void append_u64(',
            'std::uint16_t read_u16(', 'std::uint32_t read_u32(',
            'std::uint64_t read_u64(', 'bool mask_test(', 'bool mask_set(',
            'void load_state_locked(', 'void ensure_state_locked()', 'void persist_state_locked()',
            'bool commit_association_locked(', 'void grant_soul(', 'bool boss_soul_owned(', 'std::uint8_t physical_item(',
            'void associate_item(', 'std::optional<std::uint16_t> logical_item_for_actor(',
            'bool collect_extended_item(', 'void commit_item_pickup(', 'void clear_scene_actors()']:
    parts.append(function(souls, sig))
parts.append('}')
parts.append('namespace randomizer::session {')
start = session.index('struct DerivedKey')
end = session.index('bool resolve_check(', start)
parts.append(session[start:end])
parts.append(function(session, 'bool resolve_check('))
parts.append(function(session, 'void completeLogicalCheck('))
parts.append('}')
parts.append(r'''
std::unordered_map<u32,std::string> names;
ModContext* const authorizedContext = reinterpret_cast<ModContext*>(uintptr_t{1});
namespace dusk::mods {
bool s_dispatchingSilent = false, s_inFlight = false, s_inFlightSpawned = false;
u8 s_inFlightItem = 0;
struct { u32 tag = 0; } s_inFlightGive;
ItemGiveOrigin lastOrigin = ITEM_GIVE_ORIGIN_GAME;
const char* item_check_name(u32 tag) {
    const auto found = names.find(tag);
    return found == names.end() ? nullptr : found->second.c_str();
}
void complete_check(u8, u32, fopAc_ac_c*, ItemGiveOrigin origin) {
    ++nativeCompletions;
    lastOrigin = origin;
}
''')
parts.append(function(host_gives, 'void item_granted('))
parts.append(r'''
namespace svc {
void* mod_from_context(ModContext* context) {
    return context == authorizedContext ? static_cast<void*>(&names) : nullptr;
}
''')
parts.append(function(host_service, 'const char* item_check_name_for_mod('))
parts.append(function(host_service, 'ModResult item_complete_check_for_mod('))
parts.append('} }')
parts.append('using namespace randomizer;')
parts.append(function(hooks, 'HookAction hookPreExecItemGetLogicalSoul('))
parts.append(function(tools, 'u16 getItemMessageID('))
parts.append(function(tools, 'u16 getSoulItemMessageID('))
parts.append(function(hooks, 'HookAction hookPreProcCoGetItem('))
parts.append('void regularItemGrant() { ++vanillaAwards; }')
parts.append('void normalPoeGrant() { ++normalPoes; }')
parts.append('std::array<void(*)(),256> item_func_ptr_randomizer;')
parts.append('namespace randomizer::item {')
parts.append(function(item, 'void exec_item_get('))
parts.append('}')

parts.append(r'''
int main() {
    static_assert(sizeof(ItemCheckResolution) == 3);
    static_assert(is_runtime_item_id(0x2054) && !is_runtime_item_id(0x2055));
    static_assert(!is_runtime_item_id(-1) && !is_runtime_item_id(0x1202E));
    static_assert(!is_runtime_item_id(0x103) && is_runtime_item_id(0xE0));
    ItemService svc{};
    svc.check_name = dusk::mods::svc::item_check_name_for_mod;
    svc.complete_check = dusk::mods::svc::item_complete_check_for_mod;
    session::svc_mng.item = &svc;
    session::svc_mng.mod_ctx = authorizedContext;
    assert(svc.complete_check(nullptr,1,0xE0,nullptr)==MOD_INVALID_ARGUMENT);
    assert(svc.complete_check(authorizedContext,999,0xE0,nullptr)==MOD_INVALID_ARGUMENT);
    item_func_ptr_randomizer.fill(regularItemGrant);
    item_func_ptr_randomizer[dItemNo_Randomizer_POU_SPIRIT_e] = normalPoeGrant;

    // Reproduce actors registering before save-loaded is reported one second later.
    fopAc_ac_c pot{1,0}, pumpkin{2,0}, drop{3,0};
    constexpr uint64_t potKey=0x12340000, pumpkinKey=0x56780000;
    testContext.mBreakableOverrides[potKey] = {0x202E};
    testContext.mBreakableOverrides[pumpkinKey] = {0x2054};
    breakables::ensure_collection_locked();
    breakables::s_actorKeys[pot.id]=potKey;
    breakables::s_actorKeys[pumpkin.id]=pumpkinKey;
    breakables::s_spawnedItemKeys[drop.id]=potKey;
    breakables::s_pendingKeys.insert(potKey);
    session::noteSaveSlot(0,true);
    assert(breakables::is_shuffled_and_uncollected(&pot));
    assert(breakables::should_show_marker(&pumpkin));
    assert(breakables::spawned_item_assignment(&drop)==0x202E);
    auto revision = session::saveStateRevision();
    for (int i=0;i<120;++i) session::noteSaveSlot(0);
    assert(session::saveStateRevision()==revision);
    assert(breakables::should_show_marker(&pumpkin));
    session::noteSaveSlot(0,true);
    assert(breakables::spawned_item_assignment(&drop)==0x202E);
    item::exec_item_get(*breakables::spawned_item_assignment(&drop));
    breakables::collect_spawned_item(&drop);
    assert(normalPoes==0 && vanillaAwards==0);
    assert(!breakables::is_shuffled_and_uncollected(&pot));
    assert(breakables::should_show_marker(&pumpkin));
    session::noteSaveSlot(0,true);
    assert(!breakables::is_shuffled_and_uncollected(&pot));
    assert(enemy_souls::mask_test(enemy_souls::s_owned,0x202E));
    assert(!breakables::spawned_item_assignment(&drop));
    breakables::forget_actor(&pumpkin);
    assert(!breakables::should_show_marker(&pumpkin));
    breakables::clear_scene_actors();
    assert(breakables::s_actorKeys.empty() && breakables::s_spawnedItemKeys.empty());

    // Every generic reward category uses the exact wide ID, including the last Soul.
    constexpr u16 key=static_cast<u16>((43<<8)|9);
    const std::array categoryNames{"npc_gift", "chest:F_SP103:9", "freestanding:F_SP103:9",
        "poe:F_SP103:9", "bug:5", "sky:F_SP103:9", "golden_wolf:4660",
        "shop:F_SP103:0:80", "boss:F_SP103"};
    testContext.mItemLocations[categoryNames[0]]={0x2054};
    testContext.mTreasureChestOverrides[key]=0xE0;
    testContext.mTreasureChestEnemySoulOverrides[key]=0x2054;
    testContext.mFreestandingItemOverrides[key]=0xE0;
    testContext.mFreestandingEnemySoulOverrides[key]=0x2054;
    testContext.mPoeOverrides[key]=0x2054;
    testContext.mBugRewardOverrides[5]=0x2054;
    testContext.mSkyCharacterOverrides[key]=0x2054;
    testContext.mGoldenWolfOverrides[4660]=0x2054;
    testContext.mShopOverrides[(43<<16)|80]=0x2054;
    testContext.mFreestandingItemOverrides[(43<<8)|0x9F]=0xE0;
    testContext.mFreestandingEnemySoulOverrides[(43<<8)|0x9F]=0x2054;
    int index=0;
    for (const auto* name: categoryNames) {
        fopAc_ac_c giver{static_cast<u32>(20+index),static_cast<u32>(index+1)};
        names[giver.mItemGiveTag]=name;
        assert(session::logicalItemForCheck(name)==0x2054);
        ItemCheckInfo info{}; info.name=name; info.giver_actor=&giver;
        ItemCheckResolution resolution{0xFF,0xFF,false};
        assert(session::resolve_check(nullptr,&info,&resolution,nullptr));
        assert(resolution.item==0xE0);
        // A preview never grants inventory. Repeated save callbacks keep presentation.
        if (index==0) assert(!enemy_souls::mask_test(enemy_souls::s_owned,0x2054));
        session::noteSaveSlot(0,true);
        assert(enemy_souls::logical_item_for_actor(&giver)==0x2054);
        dusk::mods::s_inFlight = dusk::mods::s_inFlightSpawned = true;
        dusk::mods::s_inFlightGive.tag = giver.mItemGiveTag;
        dusk::mods::s_inFlightItem = resolution.item;
        HookArgs args{resolution.item,giver.mItemGiveTag,&giver};
        assert(hookPreExecItemGetLogicalSoul(nullptr,&args,nullptr,nullptr)==HOOK_SKIP_ORIGINAL);
        assert(!dusk::mods::s_inFlight && !dusk::mods::s_inFlightSpawned);
        assert(dusk::mods::lastOrigin==ITEM_GIVE_ORIGIN_QUEUE);
        assert(enemy_souls::mask_test(enemy_souls::s_owned,0x2054));
        assert(normalPoes==0);
        ++index;
    }
    assert(nativeCompletions==static_cast<int>(categoryNames.size()));
    assert(!session::logicalItemForCheck("missing"));

    // Silent/native grants can recover identity using the check tag without an actor.
    testContext.mItemLocations["silent_gift"]={0x2001}; names[100]="silent_gift";
    HookArgs silent{0xE0,100,nullptr};
    dusk::mods::s_dispatchingSilent = true;
    assert(hookPreExecItemGetLogicalSoul(nullptr,&silent,nullptr,nullptr)==HOOK_SKIP_ORIGINAL);
    assert(dusk::mods::lastOrigin==ITEM_GIVE_ORIGIN_QUEUE_SILENT);
    dusk::mods::s_dispatchingSilent = false;
    assert(enemy_souls::mask_test(enemy_souls::s_owned,0x2001) && normalPoes==0);
    for (const auto& definition : enemy_souls::kDefinitions) item::exec_item_get(definition.itemId);
    session::noteSaveSlot(0,true); enemy_souls::ensure_state_locked();
    for (const auto& definition : enemy_souls::kDefinitions)
        assert(enemy_souls::mask_test(enemy_souls::s_owned,definition.itemId));
    assert(normalPoes==0);
    assert(enemy_souls::mask_test(enemy_souls::s_owned,0x2054));
    assert(enemy_souls::mask_test(enemy_souls::s_owned,0x202E));
    session::noteSaveSlot(1,true); enemy_souls::ensure_state_locked();
    assert(!enemy_souls::mask_test(enemy_souls::s_owned,0x2054));
    assert(enemy_souls::s_itemAssociations.empty());
    session::noteSaveSlot(0,true); enemy_souls::ensure_state_locked();
    assert(enemy_souls::mask_test(enemy_souls::s_owned,0x2054));
    testContext.mHash="another-seed"; enemy_souls::ensure_state_locked();
    assert(!enemy_souls::mask_test(enemy_souls::s_owned,0x2054));

    // Dedicated text wins before normal Poe milestone text; ordinary Poes still count.
    daItemBase_c demo; demo.id=500; partner=&demo;
    enemy_souls::associate_item(demo.id,0x202E,std::nullopt);
    daAlink_c link; normalPoes=19;
    hookPreProcCoGetItem(nullptr,&link,nullptr,nullptr);
    assert(link.field_0x32cc==ENEMY_SOUL_MESSAGE_BASE+0x2E);
    assert(normalPoes==19);
    enemy_souls::associate_item(demo.id,0x2054,0x2000);
    enemy_souls::associate_item(demo.id,0x2054,std::nullopt);
    assert(enemy_souls::collect_extended_item(&demo));
    assert(enemy_souls::mask_test(enemy_souls::s_completed,0x2000));
    enemy_souls::clear_scene_actors(); link.field_0x32cc=0;
    hookPreProcCoGetItem(nullptr,&link,nullptr,nullptr);
    assert(link.field_0x32cc==0xE0+0x65);
    HookArgs normal{0xE0,0,&demo};
    assert(hookPreExecItemGetLogicalSoul(nullptr,&normal,nullptr,nullptr)==HOOK_CONTINUE);
    item::exec_item_get(0xE0); assert(normalPoes==20);
    constexpr std::array bossIds{dItemNo_Randomizer_DIABABA_SOUL_e,
        dItemNo_Randomizer_FYRUS_SOUL_e, dItemNo_Randomizer_MORPHEEL_SOUL_e,
        dItemNo_Randomizer_STALLORD_SOUL_e, dItemNo_Randomizer_BLIZZETA_SOUL_e,
        dItemNo_Randomizer_ARMOGOHMA_SOUL_e, dItemNo_Randomizer_ARGOROK_SOUL_e,
        dItemNo_Randomizer_ZANT_SOUL_e};
    index=0;
    for (const auto id: bossIds) {
        assert(!enemy_souls::boss_soul_owned(static_cast<u16>(id)));
        const std::string name="boss-soul-gift-"+std::to_string(index);
        testContext.mItemLocations[name]={id};
        fopAc_ac_c giver{static_cast<u32>(600+index),static_cast<u32>(200+index)};
        names[giver.mItemGiveTag]=name;
        ItemCheckInfo info{}; info.name=name.c_str(); info.giver_actor=&giver;
        ItemCheckResolution resolution{0xFF,0xFF,false};
        assert(session::resolve_check(nullptr,&info,&resolution,nullptr));
        assert(resolution.item==id);
        HookArgs bossArgs{resolution.item,giver.mItemGiveTag,&giver};
        assert(hookPreExecItemGetLogicalSoul(nullptr,&bossArgs,nullptr,nullptr)==HOOK_CONTINUE);
        // This same production award function handles pot/pumpkin assignments.
        item::exec_item_get(resolution.item);
        assert(enemy_souls::boss_soul_owned(static_cast<u16>(id)));
        assert(normalPoes==20);
        link.field_0x32cc=0; link.mProcVar2.field_0x300c=resolution.item;
        hookPreProcCoGetItem(nullptr,&link,nullptr,nullptr);
        assert(link.field_0x32cc==BOSS_SOUL_MESSAGE_BASE+index);
        ++index;
    }
    std::cout << "PASS .734: late save-loaded marker identity, durable collection and all 85 Enemy Souls, all reward categories, all eight Boss Soul permissions/text, native FIFO completion, wide-ID rejection, Poe isolation and scene cleanup.\n";
}
''')

with tempfile.TemporaryDirectory(prefix='tpr-734-tests-') as temp:
    work = Path(temp)
    cpp = work / 'souls_breakables.cpp'
    cpp.write_text('\n'.join(parts),encoding='utf-8')
    includes = [root / 'sdk/include', mod]
    if os.name == 'nt':
        compiler = shutil.which('cl')
        if compiler is None: raise RuntimeError('MSVC x64 compiler is required')
        exe = work / 'souls_breakables.exe'
        cmd = [compiler, '/nologo', '/std:c++20', '/EHsc', '/W4', '/WX', '/utf-8',
               '/D_CRT_SECURE_NO_WARNINGS', *[f'/I{p}' for p in includes], str(cpp), f'/Fe:{exe}']
    else:
        compiler = shutil.which('g++')
        if compiler is None: raise RuntimeError('g++ is required')
        exe = work / 'souls_breakables'
        cmd = [compiler, '-std=c++20', '-Wall', '-Wextra', '-Werror', '-Wno-attributes',
               *[f'-I{p}' for p in includes], str(cpp), '-o', str(exe)]
    result = subprocess.run(cmd,cwd=work,text=True,capture_output=True)
    if result.returncode:
        print(result.stdout); print(result.stderr)
        raise RuntimeError('Production .734 regression harness did not compile')
    subprocess.run([str(exe)],cwd=work,check=True)
