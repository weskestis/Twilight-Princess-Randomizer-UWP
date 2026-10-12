"""Run the real .734 grant/persistence suite plus .744 parent/display-actor ordering."""
import argparse, json, subprocess, sys, tempfile
from pathlib import Path

p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('--negative-control',action='store_true');a=p.parse_args()
control=Path(__file__).resolve().parents[1];root=a.source.resolve()
code=(control/'scripts/test-734-souls-breakables.py').read_text()
code=code.replace('root = Path(parser.parse_args().source)', 'root = Path('+json.dumps(str(root))+')')
code=code.replace('else return static_cast<HookArgs*>(args)->giver;', 'else return static_cast<T>(static_cast<HookArgs*>(args)->giver);')
code=code.replace('#include <mods/svc/item.h>', '#include <mods/svc/item.h>\n#include <mods/svc/ui.h>\n#include "src/pickup_message_guard.hpp"')
code=code.replace('struct daAlink_c {','struct daAlink_c : fopAc_ac_c {\n    enum { PROC_GET_ITEM=0x148 }; int mProcID=PROC_GET_ITEM; u32 mMsgClassID=UINT32_MAX; int checkEndMessage(u32);')
code=code.replace('bool fpcM_IsCreating(u32) { return false; }','std::unordered_set<u32> fixtureCreating;\nbool fpcM_IsCreating(u32 id) { return fixtureCreating.contains(id); }')
code=code.replace('struct { const ItemService* item; ModContext* mod_ctx; }','struct { const ItemService* item; ModContext* mod_ctx; const UiService* ui=nullptr; }')
code=code.replace("'void collect_spawned_item(', 'void forget_actor(',", "*[sig for sig in ['bool has_spawned_item_binding(', 'void inherit_spawned_item('] if sig in breakables], 'void collect_spawned_item(', 'void forget_actor(',")
code=code.replace("'void clear_scene_actors()']:", "'void clear_scene_actors()', 'bool is_collected(']:",1)
code=code.replace("'bool collect_extended_item(', 'void commit_item_pickup(',", "'void transfer_item_association(', 'bool collect_extended_item(', 'void commit_item_pickup(',")
code=code.replace("parts.append(function(item, 'void exec_item_get('))", "parts.append(function(item, 'void exec_item_get(').replace('void exec_item_get(', 'void fixture_exec_item_get(', 1))\nparts.append('void exec_item_get(u16 id) { pickupGrantTrace.push_back(id); fixture_exec_item_get(id); }')")

extra=r'''
std::vector<u16> pickupGrantTrace;
using BOOL=int;
constexpr BOOL TRUE=1,FALSE=0;
enum { fopMsg_MODE_MSG_PREPARING_e=1, fopMsg_MODE_BOX_OPENING_e=2, fopMsg_MODE_MSG_DISPLAYED_e=14, fopMsg_MODE_MSG_END_e=16, fopMsg_MODE_BOX_CLOSED_e=18, fopMsg_MODE_MSG_DESTROYED_e=19 };
struct msg_class { int mode=1; };
std::unordered_map<u32,msg_class> fixtureMessages;
u32 fixtureMessageSetResult=0;
msg_class* fopMsgM_SearchByID(u32 id){auto it=fixtureMessages.find(id);return it==fixtureMessages.end()?nullptr:&it->second;}
u32 fopMsgM_messageSet(u32,u32){return fixtureMessageSetResult;}
int bypassedNativeGrants = 0, eventResets = 0;
bool fixtureEventEnded = false;
struct FixtureCylinder { bool cleared = false;
    void SetTgType(int) {} void OffCoSPrmBit(int) {} void ClrTgHit() {} void ClrCoHit(){cleared=true;} };
struct daItem_c : fopAc_ac_c {
    enum { FLAG_DELETE_ITEM_e=1, FLAG_INIT_GET_ITEM_e=2 };
    u8 m_itemNo=0, mItemBitNo=0xFF; u32 m_item_id=UINT32_MAX;
    bool mItemOverridden=false, deleted=false; unsigned flags=0;
    int simpleStarts=0, demoStarts=0; FixtureCylinder mCcCyl;
    bool checkFlag(unsigned f)const{return (flags&f)!=0;} void setFlag(unsigned f){flags|=f;}
    void procInitSimpleGetDemo(){++simpleStarts;} void procInitGetDemoEvent(); void itemGet(); void procMainGetDemoEvent();
};
struct daDitem_c : daItemBase_c { bool dead=false; u8 field_0x93d=0x84;
    bool chkDead()const{return dead;} bool chkArgFlag(u8 flag)const{return (field_0x93d&flag)!=0;} };
struct FixturePlayer { bool checkCanoeRide(){return false;} bool checkHorseRide(){return false;} bool checkBoardRide(){return false;} };
FixturePlayer* daPy_getPlayerActorClass(){static FixturePlayer p;return &p;}
int checkItemGet(u8,int){return 0;}
void dComIfGs_offItemFirstBit(u8){}
void fopAcM_onItem(daItem_c*,u8){}
bool dComIfGp_evmng_endCheck(const char*){return fixtureEventEnded;}
void dComIfGp_event_reset(){++eventResets;}
void fopAcM_delete(daItem_c* a){a->deleted=true;}
enum { Z2SE_CONSUMP_ITEM_GET=1, Z2SE_HEART_PIECE_GET=2 };
void mDoAud_seStart(int,void*,int,int){}
void execItemGet(u8,u32,fopAc_ac_c*){++bypassedNativeGrants;}
constexpr auto dItemNo_HEART_e=dItemNo_Randomizer_HEART_e;
constexpr auto dItemNo_GREEN_RUPEE_e=dItemNo_Randomizer_GREEN_RUPEE_e;
constexpr auto dItemNo_BLUE_RUPEE_e=dItemNo_Randomizer_BLUE_RUPEE_e;
constexpr auto dItemNo_YELLOW_RUPEE_e=dItemNo_Randomizer_YELLOW_RUPEE_e;
constexpr auto dItemNo_RED_RUPEE_e=dItemNo_Randomizer_RED_RUPEE_e;
constexpr auto dItemNo_PURPLE_RUPEE_e=dItemNo_Randomizer_PURPLE_RUPEE_e;
constexpr auto dItemNo_ORANGE_RUPEE_e=dItemNo_Randomizer_ORANGE_RUPEE_e;
constexpr auto dItemNo_SILVER_RUPEE_e=dItemNo_Randomizer_SILVER_RUPEE_e;
constexpr auto dItemNo_ARROW_1_e=dItemNo_Randomizer_ARROW_1_e;
constexpr auto dItemNo_ARROW_10_e=dItemNo_Randomizer_ARROW_10_e;
constexpr auto dItemNo_ARROW_20_e=dItemNo_Randomizer_ARROW_20_e;
constexpr auto dItemNo_ARROW_30_e=dItemNo_Randomizer_ARROW_30_e;
constexpr auto dItemNo_PACHINKO_SHOT_e=dItemNo_Randomizer_PACHINKO_SHOT_e;
constexpr auto dItemNo_UTAWA_HEART_e=dItemNo_Randomizer_UTAWA_HEART_e;
constexpr auto dItemNo_KAKERA_HEART_e=dItemNo_Randomizer_KAKERA_HEART_e;
constexpr auto dItemNo_BOOMERANG_e=dItemNo_Randomizer_BOOMERANG_e;
'''
code=code.replace("message_constants =", 'stubs += '+repr(extra)+'\nmessage_constants =',1)
insert=r"""
parts.append(hooks[hooks.index('daDitem_c* s_demoPickupActor'):hooks.index('HookAction hookPreDemoItemActionEvent(')])
for sig in ['void hookPostItemGetDemoInit(', 'HookAction hookPreItemGetDemoExecute(',
            'HookAction hookPreDemoItemActionEvent(', 'void hookPostDemoItemActionEvent(',
            'HookAction hookPreItemItemGetNextExecute(', 'HookAction hookPreItemItemGet(']:
    parts.append(function(hooks,sig))
parts.append(function((root/'src/d/actor/d_a_obj_item.cpp').read_text(), 'void daItem_c::procMainGetDemoEvent()'))
if 'void recoverMissingPickupMessage(' in hooks:
    parts.append('#define HAS_PICKUP_GUARD744 1\nPickupMessageGuard s_pickupMessageGuard;')
    parts.append(function(hooks,'void recoverMissingPickupMessage('))
    parts.append(function((root/'src/d/actor/d_a_alink_demo.inc').read_text(),'BOOL daAlink_c::checkEndMessage('))
parts.append(r'''
void daItem_c::procInitGetDemoEvent(){++demoStarts;m_item_id=id+1;HookArgs a{m_itemNo,mItemGiveTag,this};hookPostItemGetDemoInit(nullptr,&a,nullptr,nullptr);}
void daItem_c::itemGet(){HookArgs a{m_itemNo,mItemGiveTag,this};if(hookPreItemItemGet(nullptr,&a,nullptr,nullptr)==HOOK_CONTINUE)execItemGet(m_itemNo,mItemGiveTag,this);}
''')
"""
probe=r'''
    // Exercise both source containers, including callback order and cancellation.
    std::vector<u16> rewards{0x2F,1,2,3,4,5,6,7,0,0x0A,0x0E,0x11,0x12,0x13,0x20,0x21,0x22,0xE0};
    for(u16 id=0x2000;id<=0x2054;++id)rewards.push_back(id);
    unsigned serial=0;
    for(unsigned kind=0;kind<2;++kind)for(u16 reward:rewards){
        ++serial;pickupGrantTrace.clear();fixtureEventEnded=false;
        breakables::clear_scene_actors();breakables::ensure_collection_locked();
        const uint64_t pickupCheckKey=0x74400000u+serial;
        testContext.mBreakableOverrides[pickupCheckKey]={reward};
        fopAc_ac_c container;container.id=10000+serial*3;
        daItem_c parent;parent.id=container.id+1;parent.m_itemNo=enemy_souls::physical_item(reward);
        breakables::s_actorKeys[container.id]=pickupCheckKey;
        breakables::s_spawnedItemKeys[parent.id]=pickupCheckKey;breakables::s_pendingKeys.insert(pickupCheckKey);
        HookArgs args{parent.m_itemNo,0,&parent};
        assert(hookPreItemItemGetNextExecute(nullptr,&args,nullptr,nullptr)==HOOK_SKIP_ORIGINAL);
        assert(parent.checkFlag(daItem_c::FLAG_INIT_GET_ITEM_e)&&parent.mCcCyl.cleared);
        if(parent.demoStarts){
            daDitem_c child;child.id=parent.m_item_id;
            if(breakables::spawned_item_assignment(&child)!=reward)return 42;
            HookArgs childArgs{parent.m_itemNo,0,&child};
            hookPreDemoItemActionEvent(nullptr,&childArgs,nullptr,nullptr);
            assert(pickupGrantTrace.empty());
            if(serial%3==0){
                // A canceled display actor must not release the parent's pending check.
                breakables::forget_spawned_item(&child);
                assert(breakables::s_pendingKeys.contains(pickupCheckKey));
                parent.itemGet();parent.itemGet();
            }else{
                if(serial%3==1){fixtureEventEnded=true;hookPreItemGetDemoExecute(nullptr,&args,nullptr,nullptr);}
                child.dead=true;const u8 flags=child.field_0x93d;
                assert(hookPreDemoItemActionEvent(nullptr,&childArgs,nullptr,nullptr)==HOOK_CONTINUE);
                assert(child.chkArgFlag(1)); // Only the native grant is suppressed.
                hookPostDemoItemActionEvent(nullptr,&childArgs,nullptr,nullptr);
                assert(child.field_0x93d==flags);
                fixtureEventEnded=true;hookPreItemGetDemoExecute(nullptr,&args,nullptr,nullptr);
                parent.procMainGetDemoEvent();assert(parent.deleted);
            }
        }else{
            assert(parent.simpleStarts==1);
            parent.itemGet(); // A repeated late callback cannot grant the item again.
        }
        assert(pickupGrantTrace==std::vector<u16>{reward});
        assert(breakables::is_collected(pickupCheckKey)&&!breakables::should_show_marker(&container));
        hookPreItemItemGetNextExecute(nullptr,&args,nullptr,nullptr);
        assert(pickupGrantTrace.size()==1);
        session::noteSaveSlot(session::currentSaveSlot(),true);
        assert(breakables::is_collected(pickupCheckKey));
    }
    assert(bypassedNativeGrants==0&&eventResets>0);
    #if HAS_PICKUP_GUARD744
    using Clock=PickupMessageGuard::Clock;using namespace std::chrono;
    const auto now=Clock::time_point{};daAlink_c pickupLink;pickupLink.id=500;
    daItemBase_c display;display.id=501;testItemEventPartner=&display;
    breakables::s_spawnedItemKeys[display.id]=0x74400001;
    BOOL ended=pickupLink.checkEndMessage(0x94);assert(!ended&&pickupLink.mMsgClassID==0);
    recoverMissingPickupMessage(&pickupLink,0x94,ended,now);assert(!ended&&pickupLink.mMsgClassID==UINT32_MAX);
    fixtureMessageSetResult=700;fixtureMessages[700].mode=fopMsg_MODE_BOX_OPENING_e;
    ended=pickupLink.checkEndMessage(0x94);recoverMissingPickupMessage(&pickupLink,0x94,ended,now+seconds(1));assert(!ended&&pickupLink.mMsgClassID==700);
    recoverMissingPickupMessage(&pickupLink,0x94,ended,now+seconds(60));assert(!ended); // A visible message is never timed out.
    fixtureMessages[700].mode=fopMsg_MODE_MSG_DISPLAYED_e;assert(!pickupLink.checkEndMessage(0x94));assert(fixtureMessages[700].mode==fopMsg_MODE_MSG_END_e);
    fixtureMessages[700].mode=fopMsg_MODE_BOX_CLOSED_e;assert(pickupLink.checkEndMessage(0x94));
    ended=FALSE;fixtureMessages[700].mode=fopMsg_MODE_MSG_PREPARING_e;
    recoverMissingPickupMessage(&pickupLink,0x95,ended,now);assert(!ended);
    recoverMissingPickupMessage(&pickupLink,0x95,ended,now+seconds(4));assert(!ended);
    recoverMissingPickupMessage(&pickupLink,0x95,ended,now+seconds(5));assert(ended&&fixtureMessages[700].mode==fopMsg_MODE_MSG_DESTROYED_e);
    ended=FALSE;fixtureMessages.clear();pickupLink.mMsgClassID=701;fixtureCreating.insert(701);
    recoverMissingPickupMessage(&pickupLink,0x96,ended,now);recoverMissingPickupMessage(&pickupLink,0x96,ended,now+seconds(60));assert(!ended&&pickupLink.mMsgClassID==701);
    fixtureCreating.clear();recoverMissingPickupMessage(&pickupLink,0x96,ended,now+seconds(61));assert(!ended&&pickupLink.mMsgClassID==UINT32_MAX);
    breakables::s_spawnedItemKeys.erase(display.id);pickupLink.mMsgClassID=0;
    recoverMissingPickupMessage(&pickupLink,0x96,ended,now+seconds(90));assert(!ended&&pickupLink.mMsgClassID==0); // Other dialogue remains native.
    #endif
    std::cout<<"PASS .744: 206 pot/pumpkin pickup lifecycles, exact wide IDs, early/late/canceled demos, native teardown, late-save and missing/visible/creating textbox protection.\n";
'''
code=code.replace("parts.append(r'''\nint main() {", insert+"\nparts.append(r'''\nint main() {",1)
code=code.replace('    std::cout << "PASS .734:',probe+'\n    std::cout << "PASS .734:',1)
code=code.replace("'/D_CRT_SECURE_NO_WARNINGS',", "'/D_CRT_SECURE_NO_WARNINGS', '/wd4100',")
code=code.replace("'-Wno-attributes',", "'-Wno-attributes', '-Wno-unused-parameter',")

with tempfile.TemporaryDirectory(prefix='pickup-744-baseline-') as name:
    if a.negative_control:
        baseline=Path(name)
        for relative in ['mods/randomizer/src/breakables.cpp','mods/randomizer/src/breakables.hpp',
                         'mods/randomizer/src/hooks.cpp','src/d/actor/d_a_obj_item.cpp', 'mods/randomizer/src/pickup_message_guard.hpp']:
            f=baseline/relative;f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes((root/relative).read_bytes().replace(b'\r\n',b'\n'))
        subprocess.run(['git','-C',str(baseline),'apply','--reverse',str(control/'patches/xbox-pickup-lifecycle-744.patch')],check=True)
        for variable,relative in [('hooks','mods/randomizer/src/hooks.cpp'),('breakables','mods/randomizer/src/breakables.cpp')]:
            anchor='host_gives = '
            code=code.replace(anchor,variable+' = Path('+json.dumps(str(baseline/relative))+').read_text()\n'+anchor,1)
    try:
        exec(compile(code,str(control/'scripts/test-734-souls-breakables.py'),'exec'),{'__file__':str(control/'scripts/test-734-souls-breakables.py')})
    except subprocess.CalledProcessError as e:
        if not a.negative_control or e.returncode!=42:raise
        print('PASS: shipped .743 loses the exact breakable assignment on its get-item display actor')
    else:
        if a.negative_control:raise RuntimeError('Shipped .743 pickup handoff defect was not reproduced')
