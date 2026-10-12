"""Compile the production Soul-room query and both native combat-reward actors."""
import argparse, os, re, shutil, subprocess, tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('--negative-control',action='store_true');a=p.parse_args()
root=a.source.resolve();control=Path(__file__).resolve().parents[1]
def function(source,signature):
    start=source.index(signature);depth=0;opened=False
    for m in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/|[{}]',source[start:]):
        if m.group()=='{':depth+=1;opened=True
        elif m.group()=='}':
            depth-=1
            if opened and depth==0:return source[start:start+m.end()]
    raise RuntimeError(signature)
with tempfile.TemporaryDirectory(prefix='grotto-744-') as name:
    work=Path(name);host=root
    if a.negative_control:
        host=work/'baseline'
        for path in ['mods/randomizer/src/enemy_souls.cpp','mods/randomizer/src/enemy_souls.hpp','src/d/actor/d_a_alldie.cpp','src/d/actor/d_a_tbox.cpp']:
            f=host/path;f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes((root/path).read_bytes().replace(b'\r\n',b'\n'))
        patch=work/'grotto-rollback.patch';patch.write_bytes((control/'patches/xbox-grotto-rewards-744.patch').read_bytes().replace(b'\r\n',b'\n'))
        subprocess.run(['git','-C',str(host),'apply','--reverse',str(patch)],check=True)
    souls=(root/'mods/randomizer/src/enemy_souls.cpp').read_text()
    cpp=r'''
#include <cassert>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <cstdio>
using s8=int8_t;using u32=uint32_t;
constexpr int fopAc_ENEMY_e=2;
struct fopAc_ac_c {u32 id=0;int room=0,group=2;};
std::unordered_map<u32,fopAc_ac_c*> actors;
bool active=true,failQuery=false;unsigned switches=0,events=0,escapeQueries=0;
bool randomizer_IsActive(){return active;}
fopAc_ac_c* fopAcM_SearchByID(u32 id){return actors.contains(id)?actors[id]:nullptr;}
int fopAcM_GetRoomNo(const fopAc_ac_c* a){return a->room;}
int fopAcM_GetGroup(const fopAc_ac_c* a){return a->group;}
namespace randomizer::enemy_souls {
std::mutex s_mutex;std::unordered_set<u32> s_blockedActors;
void ensure_state_locked(){if(failQuery)throw std::runtime_error("state unavailable");}
/*ROOM_QUERY*/
}
extern "C" bool tpr_room_has_pending_soul_enemies(s8 room)noexcept{return randomizer::enemy_souls::room_has_pending_enemies(room);}
fopAc_ac_c* fopAcM_myRoomSearchEnemy(int room){
 ++escapeQueries;for(auto [id,a]:actors)if(a->room==room&&a->group==2&&!randomizer::enemy_souls::s_blockedActors.contains(id))return a;return nullptr;
}
void dComIfGs_onSwitch(int,int){++switches;}
struct daAlldie_c:fopAc_ac_c {
 enum{ACT_CHECK,ACT_TIMER,ACT_WAIT,ACT_ORDER};int mAction=ACT_CHECK,mTimer=0,mEventIdx=-1;
 int getSwbit(){return 7;}int actionCheck();int actionTimer();
};
struct EventInfo {bool accepted=false;bool checkCommandDemoAccrpt(){return accepted;}void onCondition(int){}}
;
struct daTbox_c:fopAc_ac_c {
 EventInfo eventInfo;int mTimer=65,mStaffId=0,mEventId=4,event=255,action=0;
 int actionGenocide();int actionDemo2(){return 1;}void setAction(int(daTbox_c::*)()){++action;}
 int getEvent(){return event;}int getSwNo(){return 9;}void demoProc(){++action;}
};
constexpr const char* l_staff_name="TBOX";
int dComIfGp_evmng_getMyStaffId(const char*,void*,int){return 1;}
void fopAcM_orderOtherEvent(daTbox_c*,const char*,int,int,int){++events;}
void fopAcM_orderOtherEventId(daTbox_c*,int,int,int,int,int){++events;}
/*ACTOR_FUNCTIONS*/
int main(){
 using namespace randomizer::enemy_souls;
 fopAc_ac_c enemy{100,3,2},other{101,4,2},prop{102,3,1};
 actors[100]=&enemy;s_blockedActors.insert(100);s_blockedActors.insert(999);
 assert(fopAcM_myRoomSearchEnemy(3)==nullptr); // Existing escape/door behavior is unchanged.
 assert(room_has_pending_enemies(3)&&!room_has_pending_enemies(4)&&!room_has_pending_enemies(-1));
 active=false;assert(!room_has_pending_enemies(3));active=true;
 actors[101]=&other;actors[102]=&prop;s_blockedActors.insert(101);s_blockedActors.insert(102);
 assert(room_has_pending_enemies(4)&&!room_has_pending_enemies(5));
 daAlldie_c clear;clear.room=3;daTbox_c chest;chest.room=3;
 for(int i=0;i<500;++i){clear.actionCheck();chest.actionGenocide();}
 if(clear.mAction!=daAlldie_c::ACT_CHECK||chest.mTimer!=65||switches||events)return 42;
 clear.mAction=daAlldie_c::ACT_TIMER;clear.mTimer=1;clear.actionTimer();assert(clear.mAction==daAlldie_c::ACT_CHECK&&!switches);
 s_blockedActors.erase(100); // Finding the Soul permits spawning; the living enemy still must be defeated.
 assert(!room_has_pending_enemies(3)&&fopAcM_myRoomSearchEnemy(3)==&enemy);
 clear.actionCheck();chest.actionGenocide();assert(clear.mAction==daAlldie_c::ACT_CHECK&&chest.mTimer==65&&!events);
 actors.erase(100);assert(!room_has_pending_enemies(3));
 clear.actionCheck();assert(clear.mAction==daAlldie_c::ACT_TIMER&&clear.mTimer==65);
 for(int i=0;i<65;++i){clear.actionTimer();chest.actionGenocide();assert(!switches&&!events);}
 clear.actionTimer();chest.actionGenocide();assert(clear.mAction==daAlldie_c::ACT_WAIT&&switches==2&&events==1);
 clear.mAction=daAlldie_c::ACT_CHECK;clear.mEventIdx=10;clear.actionCheck();clear.mTimer=0;clear.actionTimer();assert(clear.mAction==daAlldie_c::ACT_ORDER);
 chest.event=2;chest.actionGenocide();assert(events==2);
 chest.eventInfo.accepted=true;chest.actionGenocide();assert(chest.action==2);
 failQuery=true;assert(room_has_pending_enemies(3));clear.mAction=daAlldie_c::ACT_CHECK;clear.actionCheck();assert(clear.mAction==daAlldie_c::ACT_CHECK);
 puts("PASS .744: absent Souls block native all-die/chest rewards, living enemies still count, genuine defeat retains native timers/events, escape behavior is preserved");
}
'''
    cpp=cpp.replace('/*ROOM_QUERY*/',function(souls,'bool room_has_pending_enemies('))
    cpp=cpp.replace('/*ACTOR_FUNCTIONS*/','\n'.join(function((host/file).read_text(),sig) for file,sig in [('src/d/actor/d_a_alldie.cpp','int daAlldie_c::actionCheck()'),('src/d/actor/d_a_alldie.cpp','int daAlldie_c::actionTimer()'),('src/d/actor/d_a_tbox.cpp','int daTbox_c::actionGenocide()')]))
    file=work/'combat.cpp';file.write_text(cpp);exe=work/('combat.exe' if os.name=='nt' else 'combat');compiler=shutil.which('cl' if os.name=='nt' else 'c++')
    cmd=[compiler,'/nologo','/std:c++20','/EHsc',str(file),'/Fe:'+str(exe)] if os.name=='nt' else [compiler,'-std=c++20','-O1',str(file),'-o',str(exe)]
    subprocess.run(cmd,cwd=work,check=True);result=subprocess.run([str(exe)])
    if a.negative_control:
        if result.returncode!=42:raise RuntimeError('The .743 native reward bug was not reproduced')
        print('PASS negative control: .743 native all-die/chest rewards treat hidden Soul enemies as defeated')
    else:result.check_returncode()
