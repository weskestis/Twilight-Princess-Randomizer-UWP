"""Exercise the native optimized pickup message path without a checkEndMessage detour."""
import argparse, os, subprocess, tempfile
from pathlib import Path
p = argparse.ArgumentParser(); p.add_argument('source', type=Path); p.add_argument('--negative-control', action='store_true')
a = p.parse_args(); r = a.source.resolve()

def function(text, signature):
    start = text.index(signature); brace = text.index('{', start); depth = 1; pos = brace + 1
    while depth:
        depth += (text[pos] == '{') - (text[pos] == '}'); pos += 1
    return text[start:pos] + '\n'

native = (r / 'src/d/actor/d_a_alink_demo.inc').read_text()
hooks = (r / 'mods/randomizer/src/hooks.cpp').read_text()
cpp = r'''
#include "pickup_message_guard.hpp"
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>
using BOOL = int; using u32 = uint32_t; using u8 = uint8_t;
constexpr BOOL TRUE=1, FALSE=0;
constexpr u32 fpcM_ERROR_PROCESS_ID_e=UINT32_MAX;
enum { fopMsg_MODE_MSG_PREPARING_e=1, fopMsg_MODE_BOX_OPENING_e=2, fopMsg_MODE_MSG_TYPING_e=6,
    fopMsg_MODE_MSG_DISPLAYED_e=14, fopMsg_MODE_MSG_END_e=16, fopMsg_MODE_BOX_CLOSED_e=18, fopMsg_MODE_MSG_DESTROYED_e=19 };
struct fopAc_ac_c { u32 id=0; };
struct daAlink_c : fopAc_ac_c {
    enum { PROC_GET_ITEM=0x148 }; int mProcID=PROC_GET_ITEM;
    u32 mMsgClassID=fpcM_ERROR_PROCESS_ID_e, field_0x32cc=0x94;
    struct { u8 field_0x300c=0xE0; } mProcVar2;
    BOOL checkEndMessage(u32);
};
fopAc_ac_c* partner = nullptr;
u32 fopAcM_GetID(const fopAc_ac_c* actor) { return actor->id; }
u32 fpcM_GetID(const fopAc_ac_c* actor) { return actor->id; }
fopAc_ac_c* fopAcM_getItemEventPartner(daAlink_c*) { return partner; }
std::unordered_set<u32> creating, bound;
bool fpcM_IsCreating(u32 id) { return creating.contains(id); }
namespace randomizer::breakables { bool has_spawned_item_binding(const fopAc_ac_c* actor) { return bound.contains(actor->id); } }
std::unordered_map<u32,uint16_t> soulBindings;
namespace randomizer::enemy_souls {
std::optional<uint16_t> logical_item_for_actor(const fopAc_ac_c* actor) {
    auto found=soulBindings.find(actor->id); return found==soulBindings.end()?std::nullopt:std::optional(found->second);
}
bool is_enemy_soul(uint16_t id) { return id>=0x2000&&id<0x2055; }
u8 physical_item(uint16_t id) { return is_enemy_soul(id)?0xE0:static_cast<u8>(id); }
}
bool randomizer_IsBossSoulItem(u8 id) { return id>=0x78&&id<=0x7F; }
struct msg_class { int mode=fopMsg_MODE_MSG_PREPARING_e; };
msg_class message;
bool messageExists = false;
u32 messageSetResult = 0;
msg_class* fopMsgM_SearchByID(u32 id) { return messageExists && id==700 ? &message : nullptr; }
u32 fopMsgM_messageSet(u32,u32) { return messageSetResult; }
namespace mods::log { template <typename... T> void warn(T&&...) {} }
struct UiToastDesc { const char* title_rml=nullptr; const char* body_rml=nullptr; int duration_ms=0; };
#define UI_TOAST_DESC_INIT {}
struct Ui { void push_toast(void*, UiToastDesc*) {} };
namespace randomizer {
namespace messages { u32 pickup_message_id(u32 id) noexcept { return id; } }
namespace session { struct { Ui* ui=nullptr; void* mod_ctx=nullptr; } svc_mng; }
PickupMessageGuard s_pickupMessageGuard;
PickupMessageGuard::Clock::time_point fixtureNow;
'''
cpp += function(hooks, 'void recoverMissingPickupMessage(')
bridge = 'extern "C" BOOL tpr_complete_breakable_pickup_message('
if bridge in hooks:
    cpp += function(hooks, bridge).replace('PickupMessageGuard::Clock::now()', 'fixtureNow')
resolver='extern "C" u32 tpr_resolve_soul_pickup_message('
if resolver in hooks: cpp += function(hooks,resolver)
cpp += '}\n'
cpp += function(native, 'BOOL daAlink_c::checkEndMessage(')
if 'BOOL pickupMessageFinished = FALSE;' in native:
    start = native.index('BOOL pickupMessageFinished = FALSE;')
    step = native[start:native.index('if (pickupMessageFinished)', start)]
    step = step.replace('field_0x32cc', 'link->field_0x32cc').replace('checkEndMessage(', 'link->checkEndMessage(').replace('this,', 'link,')
    cpp += 'using randomizer::tpr_complete_breakable_pickup_message;\n'
    if resolver in hooks: cpp += 'using randomizer::tpr_resolve_soul_pickup_message;\n'
    cpp += 'BOOL nativeStep(daAlink_c* link) {\n' + step + '\nreturn pickupMessageFinished; }\n'
else:
    assert a.negative_control
    assert 'if (field_0x32cc != 0 && checkEndMessage(field_0x32cc))' in native
    cpp += 'BOOL nativeStep(daAlink_c* link) { return link->field_0x32cc != 0 && link->checkEndMessage(link->field_0x32cc); }\n'
cpp += r'''
int main() {
    using namespace randomizer; using namespace std::chrono;
    daAlink_c link; link.id=500; fopAc_ac_c item; item.id=501; partner=&item; bound.insert(item.id);
    // No detour is installed: the actual native call site must reach the guard on its own.
    for (unsigned kind=0;kind<2;++kind) {
        s_pickupMessageGuard.reset(); messageExists=false; messageSetResult=0; link.mMsgClassID=UINT32_MAX;
        bool ended=false;
        for (int tick=0;tick<=600;++tick) {
            fixtureNow=PickupMessageGuard::Clock::time_point{}+milliseconds(tick*10);
            if (nativeStep(&link)) { assert(tick>=500); ended=true; break; }
        }
        if (!ended) return 42;
        assert(bound.contains(item.id));
    }
    s_pickupMessageGuard.reset(); messageExists=true; messageSetResult=700; link.mMsgClassID=700;
    message.mode=fopMsg_MODE_MSG_DESTROYED_e; fixtureNow=PickupMessageGuard::Clock::time_point{};
    assert(!nativeStep(&link)); fixtureNow+=seconds(5); assert(nativeStep(&link));
    // Shop presentation actors have a Soul sidecar, not a pot/pumpkin binding.
    bound.clear(); messageExists=false; messageSetResult=0;
    for (uint16_t soul=0x2000;soul<0x2055;++soul) {
        soulBindings[item.id]=soul; link.mProcVar2.field_0x300c=0xE0;
        link.field_0x32cc=0x7F20+(soul-0x2000); link.mMsgClassID=UINT32_MAX;
        s_pickupMessageGuard.reset(); fixtureNow=PickupMessageGuard::Clock::time_point{};
        assert(!nativeStep(&link)); fixtureNow+=seconds(5); assert(nativeStep(&link));
        assert(soulBindings.at(item.id)==soul); // Recovery retains the exact wide award.
    }
    soulBindings.clear();
    for (u8 soul=0x78;soul<=0x7F;++soul) {
        link.mProcVar2.field_0x300c=soul; link.field_0x32cc=0x7F00+(soul-0x78); link.mMsgClassID=UINT32_MAX;
        s_pickupMessageGuard.reset(); fixtureNow=PickupMessageGuard::Clock::time_point{};
        assert(!nativeStep(&link)); fixtureNow+=seconds(5); assert(nativeStep(&link));
    }
    // A stale or mismatched sidecar cannot release an unrelated pickup.
    soulBindings[item.id]=0x202F; link.mProcVar2.field_0x300c=0x01;
    s_pickupMessageGuard.reset(); link.mMsgClassID=0;
    fixtureNow=PickupMessageGuard::Clock::time_point{}; assert(!nativeStep(&link));
    fixtureNow+=seconds(60); assert(!nativeStep(&link)); assert(link.mMsgClassID==0);
    soulBindings.clear(); bound.insert(item.id); link.mProcVar2.field_0x300c=0xE0;
    messageExists=true; messageSetResult=700;
    // Visible messages, still-creating actors and unrelated dialogue remain native.
    for (const int mode : {fopMsg_MODE_BOX_OPENING_e, fopMsg_MODE_MSG_TYPING_e, fopMsg_MODE_MSG_END_e}) {
        s_pickupMessageGuard.reset(); message.mode=mode; link.mMsgClassID=700;
        fixtureNow=PickupMessageGuard::Clock::time_point{}; assert(!nativeStep(&link));
        fixtureNow+=seconds(60); assert(!nativeStep(&link)); assert(message.mode==mode);
    }
    message.mode=fopMsg_MODE_MSG_DISPLAYED_e; assert(!nativeStep(&link)); assert(message.mode==fopMsg_MODE_MSG_END_e);
    message.mode=fopMsg_MODE_BOX_CLOSED_e; assert(nativeStep(&link));
    messageExists=false; link.mMsgClassID=701; creating.insert(701);
    fixtureNow=PickupMessageGuard::Clock::time_point{}; assert(!nativeStep(&link)); fixtureNow+=seconds(60); assert(!nativeStep(&link));
    creating.clear(); bound.clear(); link.mMsgClassID=0;
    assert(!nativeStep(&link)); fixtureNow+=seconds(60); assert(!nativeStep(&link));
    assert(link.mMsgClassID==0);
    puts("PASS: native optimized pickup recovers pots, pumpkins, all 85 shop Enemy Souls including Huge Poe and eight Boss Souls; preserves exact bindings and visible/creating/unrelated dialogue");
}
'''
with tempfile.TemporaryDirectory(prefix='pickup-recovery-748-') as temp:
    w=Path(temp); source=w/'test.cpp'; source.write_text(cpp,encoding='utf-8'); exe=w/('test.exe' if os.name=='nt' else 'test')
    include=r/'mods/randomizer/src'
    command=['cl','/nologo','/EHsc','/std:c++20','/O2','/D_UWP=1','/I'+str(include),str(source),'/Fe:'+str(exe)] if os.name=='nt' else ['c++','-std=c++20','-O2','-D_UWP=1','-I'+str(include),str(source),'-o',str(exe)]
    subprocess.run(command,cwd=w,check=True); result=subprocess.run([str(exe)],cwd=w)
    if a.negative_control:
        assert result.returncode==42,'Shipped .747 unexpectedly recovers the unhooked native pickup path'
        print('PASS: shipped .747 native pickup path remains stuck when its detour is bypassed')
    else:
        result.check_returncode()
