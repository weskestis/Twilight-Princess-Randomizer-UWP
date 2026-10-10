"""Compile production hint/UI/reset/card bodies and reproduce shipped .740 failures."""
from __future__ import annotations
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def function(source, signature):
    start = source.index(signature)
    token = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/|[{}]')
    depth = 0
    for match in token.finditer(source, start):
        if match.group() == '{': depth += 1
        elif match.group() == '}':
            depth -= 1
            if depth == 0: return source[start:match.end()]
    raise RuntimeError('Unclosed function: '+signature)

p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('--negative-control',action='store_true')
a=p.parse_args();root=a.source.resolve();control=Path(__file__).resolve().parents[1]
compiler=shutil.which('cl' if os.name=='nt' else 'g++')
if not compiler: raise RuntimeError('C++ compiler unavailable')
with tempfile.TemporaryDirectory(prefix='tpr-741-native-') as temporary:
 work=Path(temporary);scanroot=root
 if a.negative_control:
  scanroot=work/'old';scanroot.mkdir()
  for kind,sub in [('host',''),('randomizer','mods/randomizer'),('aurora','extern/aurora')]:
   patch=control/'patches'/f'xbox-generation-reset-save-741-{kind}.patch'
   destination=scanroot/sub;destination.mkdir(parents=True,exist_ok=True)
   for line in patch.read_text().splitlines():
    if line.startswith('+++ b/'):
     name=line[6:];out=destination/name;out.parent.mkdir(parents=True,exist_ok=True)
     out.write_bytes((root/sub/name).read_bytes())
   subprocess.run(['git','-C',str(destination),'apply','--reverse',str(patch)],check=True)
 def read(name):return (scanroot/name).read_text(encoding='utf-8')
 def run(name,body,expected_failure=None):
  source=work/(name+'.cpp');source.write_text(body,encoding='utf-8');exe=work/(name+('.exe' if os.name=='nt' else ''))
  cmd=([compiler,'/nologo','/std:c++20','/EHsc','/utf-8','/I'+str(root/'sdk/include'),str(source),'/Fe:'+str(exe)] if os.name=='nt' else [compiler,'-std=c++20','-O1','-pthread','-I'+str(root/'sdk/include'),str(source),'-o',str(exe)])
  subprocess.run(cmd,check=True,cwd=work)
  result=subprocess.run([str(exe),str(work/(name+'-data'))],capture_output=True,text=True,timeout=20)
  print(result.stdout,end='');print(result.stderr,end='')
  if expected_failure:
   if result.returncode==0 or expected_failure not in result.stderr:raise RuntimeError(name+' negative control did not reproduce '+expected_failure)
   print('PASS .740 negative control:',expected_failure)
  elif result.returncode:raise RuntimeError(name+' production regression')
 common=r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>
void require(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
#define LOG_TO_DEBUG(...)
'''
 # Missing text for authored breakable checks was a real Max-hints reroll failure.
 location_text=read('mods/randomizer/generator/logic/hints.cpp')
 generated=function(location_text,'static Text GetGeneratedLocationTextObject(') if 'static Text GetGeneratedLocationTextObject(' in location_text else ''
 text_driver=common+r'''
#include <set>
struct Text {enum Type {STANDARD,PRETTY};std::string value;Text(std::string v):value(std::move(v)){}Text& operator+=(const std::string& s){value+=s;return *this;}Text& operator+=(const Text& s){value+=s.value;return *this;}};
struct Item{std::string name="Green Rupee";const std::string& GetName(){return name;}};
struct Area{std::vector<std::string> GetHintRegions(){return {"Arbiters Grounds"};}};
struct Access{Area area;Area* GetArea(){return &area;}};
namespace location{struct Location{std::string name;std::unordered_set<std::string> categories;Item item;Access access;
 const std::string& GetName(){return name;}Item* GetOriginalItem(){return &item;}bool HasCategories(const char* c){return categories.contains(c);}std::vector<Access*> GetAccessList(){return {&access};}};}
bool textObjectExists(const std::string& n){return n=="Arbiters Grounds"||n=="Original Check";}
Text getTextObject(const std::string& n,Text::Type){if(!textObjectExists(n))throw std::runtime_error("missing text");return Text{n};}
Text getTextObjectForTemplate(const std::string& n,Text::Type t,const std::string&){return getTextObject(n,t);}
Text makeTextListing(const std::vector<Text>& ts){return ts.front();}
'''+generated+'\n'+function(location_text,'static Text GetLocationTextObjectForTemplate(')+r'''
int main(){try{location::Location pot{"Pot 0323 (D_MN05, Room 11)",{"Breakable"}};
 Text text{"empty"};try{text=GetLocationTextObjectForTemplate(&pot,Text::PRETTY,"Location Hint");}catch(...){throw std::runtime_error("MISSING_AUTHORED_POT_HINT_TEXT");}
 require(text.value.find("Pot 0323")!=std::string::npos&&text.value.find("Arbiters Grounds")!=std::string::npos&&text.value.find("Room 11")!=std::string::npos,"authored pot retains unique check/region/room");
 location::Location pumpkin{"Pumpkin 0042 (F_SP103, Room 0)",{"Breakable"}};require(GetLocationTextObjectForTemplate(&pumpkin,Text::PRETTY,"Location Hint").value.find("Pumpkin 0042")!=std::string::npos,"pumpkin remains hintable");
 location::Location enemy{"First Defeat - Darknut",{"Enemy First Defeat"}};require(GetLocationTextObjectForTemplate(&enemy,Text::PRETTY,"Location Hint").value.find("Darknut")!=std::string::npos,"first defeat remains hintable");
 location::Location original{"Original Check",{}};require(GetLocationTextObjectForTemplate(&original,Text::PRETTY,"Location Hint").value=="Original Check","existing static text preserved");
 location::Location unknown{"Invalid Static Check",{}};bool rejected=false;try{GetLocationTextObjectForTemplate(&unknown,Text::PRETTY,"Location Hint");}catch(...){rejected=true;}require(rejected,"unknown static names stay validated");
 std::cout<<"PASS .741 authored pot/pumpkin/first-defeat hint text, readable region/room, static text preservation and unknown-static rejection\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
'''
 run('authored_hint_text',text_driver,'MISSING_AUTHORED_POT_HINT_TEXT' if a.negative_control else None)
 # The search boundary is controlled; the matching and item restoration are production.
 hints=common+r'''
namespace item {struct Item {};}
namespace location {struct Location;using LocationPool=std::vector<Location*>;}
struct World;struct Randomizer {std::vector<int> worlds;auto& GetWorlds(){return worlds;}};
namespace location {struct Location {item::Item item;item::Item* current=&item;bool sign=false;World* world;
 item::Item* GetCurrentItem(){return current;}void RemoveCurrentItem(){current=nullptr;}void SetCurrentItem(item::Item* i){current=i;}
 World* GetWorld(){return world;}bool HasCategories(const char*){return sign;}};}
struct Text {enum {ENGLISH};struct Entry{std::string str;bool operator==(const Entry&)const=default;};std::array<Entry,1> mEntries;bool operator==(const Text&)const=default;};
struct PathHint {location::Location* goalLocation;location::Location* hintedLocation;bool operator==(const PathHint&)const=default;};
struct ItemHint {location::Location* location;bool operator==(const ItemHint&)const=default;};
struct LocationHint {location::Location* location;bool operator==(const LocationHint&)const=default;};
struct BarrenHint {std::string region;bool operator==(const BarrenHint&)const=default;};
struct Hint {Text text;std::variant<std::monostate,PathHint,ItemHint,LocationHint,BarrenHint> data;location::Location* reservedSign=nullptr;bool operator==(const Hint&)const=default;};
struct World {Randomizer r;std::unordered_map<location::Location*,std::vector<Hint>> hints;Randomizer* GetRandomizer(){return &r;}auto& GetHintSignHints(){return hints;}};
namespace utility {namespace random {template<class T>void ShufflePool(T&) {}}namespace container {
 template<class T,class V>bool ElementInContainer(const T& c,const V& v){return std::find(c.begin(),c.end(),v)!=c.end();}
 template<class T,class V>void Erase(T& c,const V& v){std::erase(c,v);}}}
namespace search {bool throwSearch=false;std::vector<location::Location*> all;
 std::unordered_map<location::Location*,location::LocationPool> graph;
 struct Search {std::unordered_set<location::Location*> _visitedLocations;static Search Accessible(std::vector<int>*){return {};}
 void SearchWorlds(){if(throwSearch)throw std::runtime_error("cancelled search");_visitedLocations={all.begin(),all.end()};
 for(const auto& [loc,signs]:graph)if(!loc->current){_visitedLocations={signs.begin(),signs.end()};break;}}};
'''+function(read('mods/randomizer/generator/logic/search.cpp'),'location::LocationPool GetPossibleHintSigns(location::Location* location)')+r'''
}
struct HintGenerator {World* _world;void AssignHintSignHints(const location::LocationPool&,std::vector<Hint>);};
'''+function(read('mods/randomizer/generator/logic/hints.cpp'),'void HintGenerator::AssignHintSignHints(')+r'''
int main(){try{World w;location::Location s0,s1,s2;for(auto* s:{&s0,&s1,&s2}){s->sign=true;s->world=&w;}
 search::all={&s0,&s1,&s2};HintGenerator g{&w};std::array<location::Location,5> locs;std::vector<Hint> hints;
 for(size_t i=0;i<locs.size();++i){locs[i].world=&w;search::graph[&locs[i]]={i<4?&s0:&s1};Hint h;h.text.mEntries[0].str=std::to_string(i);h.data=LocationHint{&locs[i]};hints.push_back(h);}
 try{g.AssignHintSignHints(search::all,hints);}catch(...){throw std::runtime_error("FIXED_CAPACITY_REJECTS_REACHABLE_HINTS");}
 for(size_t i=0;i<hints.size();++i){bool reachable=false;for(auto* sign:search::graph[&locs[i]])reachable|=utility::container::ElementInContainer(w.hints[sign],hints[i]);require(reachable,"hint must precede its own item");require(locs[i].current==&locs[i].item,"search restores hinted item");}
 // Existing unequal occupancy must not force unbounded duplicate filling.
 w.hints.clear();w.hints[&s1]={hints[0],hints[1],hints[2],hints[3]};g.AssignHintSignHints(search::all,{hints[4]});
 for(const auto& [sign,list]:w.hints)for(size_t i=0;i<list.size();++i)for(size_t j=i+1;j<list.size();++j)require(!(list[i]==list[j]),"no identical copies on same sign");
 search::throwSearch=true;try{search::GetPossibleHintSigns(&locs[0]);}catch(const std::runtime_error&){}require(locs[0].current==&locs[0].item,"cancellation must restore world item");search::throwSearch=false;
 search::graph[&locs[0]]={};auto before=w.hints;bool rejected=false;try{g.AssignHintSignHints(search::all,{hints[0]});}catch(...){rejected=true;}require(rejected&&before==w.hints,"truly unreachable hint remains rejected without publication");
 std::cout<<"PASS .741 adaptive logical matching, bounded copies, unequal occupied signs, cancellation restoration and unreachable rejection\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
'''
 run('hints',hints,'FIXED_CAPACITY_REJECTS_REACHABLE_HINTS' if a.negative_control else None)
 ui=read('mods/randomizer/src/ui/rando_seed_generation.cpp')
 ui=ui[ui.index('namespace randomizer::ui {'):]
 header=read('mods/randomizer/src/ui/rando_seed_generation.hpp');header=header[header.index('namespace randomizer::ui {'):]
 ui_driver=common+r'''
#include <mods/svc/ui.h>
namespace mods::log {template<class... T>void debug(const char*,const T&...){}template<class... T>void error(const char*,const T&...){} }
constexpr int Z2SE_SY_MENU_BACK=1,Z2SE_SY_FILE_SAVE_OK=2,Z2SE_SYS_RESULT_WRONG=3;
void mDoAud_seStartMenu(int){}
namespace randomizer::session {struct Manager{ModContext* mod_ctx=nullptr;UiService* ui=nullptr;}svc_mng;}
namespace randomizer::ui {void refresh_play_seed_options(bool){} }
std::atomic<int> jobs=0,active=0,maxActive=0;std::atomic<bool> blocked=true,failWorker=false,throwWorker=false;
'''+header+r'''
namespace randomizer {bool GenerateAndWriteSeed(){++jobs;int n=++active;maxActive.store(std::max(maxActive.load(),n));
 while(blocked.load() && !ui::IsSeedGenerationCancelled())std::this_thread::yield();
 --active;if(throwWorker.load())throw std::runtime_error("test failure");return !failWorker.load();}}
'''+ui+r'''
using namespace randomizer::ui;
UiService service{};UiDialogHandle live=0,nextHandle=1;UiDialogDesc last{};bool pushFails=false;
std::thread::id mainThread;
ModResult push(ModContext*,const UiDialogDesc* desc,UiDialogHandle* out){require(std::this_thread::get_id()==mainThread,"dialog UI thread");if(pushFails)return MOD_ERROR;last=*desc;live=*out=nextHandle++;return desc->build(nullptr,1,nullptr,nullptr);}
ModResult progress(ModContext*,UiElementHandle,float,UiElementHandle* out){*out=100+live;return MOD_OK;}
ModResult closeDialog(ModContext*,UiDialogHandle h){if(live==h)live=0;return MOD_OK;}
ModResult body(ModContext*,UiDialogHandle h,const char*){return h==live?MOD_OK:MOD_INVALID_ARGUMENT;}
ModResult icon(ModContext*,UiDialogHandle h,const char*){return h==live?MOD_OK:MOD_INVALID_ARGUMENT;}
ModResult setProgress(ModContext*,UiElementHandle,float){require(std::this_thread::get_id()==mainThread,"progress must use UI thread");return MOD_OK;}
void tickUntil(const std::function<bool()>& done){auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(!done()){UpdateSeedGenerationDialog();std::this_thread::yield();require(std::chrono::steady_clock::now()<deadline,"worker completion deadline");}}
void back(){auto h=live;require(last.on_dismiss!=nullptr,"MISSING_BACK_DISMISS_CALLBACK");last.on_dismiss(nullptr,h,nullptr);closeDialog(nullptr,h);}
int main(){try{mainThread=std::this_thread::get_id();service.dialog_push=push;service.pane_add_progress=progress;service.dialog_close=closeDialog;service.dialog_set_body=body;service.dialog_set_icon=icon;service.elem_set_progress=setProgress;randomizer::session::svc_mng.ui=&service;
 GenerateRandomizerSeed();auto old=live;tickUntil([]{return jobs.load()==1;});back();require(seedGenDialog==0&&seedGenProgressBar==0,"Back clears dialog and progress handles");
 GenerateRandomizerSeed();blocked=false;tickUntil([]{return jobs.load()==2&&seedGenStatus.load()==SeedGenerateStatus::Ready;});require(maxActive==1,"retry must not overlap cancelled worker");require(live!=0,"retry completion can be dismissed");OnDialogActionOK(nullptr,old,nullptr);require(live==seedGenDialog&&live!=0,"stale callback must not dismiss new dialog");back();
 failWorker=true;GenerateRandomizerSeed();tickUntil([]{return seedGenStatus.load()==SeedGenerateStatus::Ready;});back();require(seedGenDialog==0,"failed seed can Back and retry");failWorker=false;throwWorker=true;
 GenerateRandomizerSeed();tickUntil([]{return seedGenStatus.load()==SeedGenerateStatus::Ready;});require(ReadGenerationStatusMsg().find("test failure")!=std::string::npos,"worker exception is visible");back();throwWorker=false;
 GenerateRandomizerSeed();tickUntil([]{return seedGenStatus.load()!=SeedGenerateStatus::Generating;});live=0;UpdateSeedGenerationDialog();require(seedGenDialog==0&&seedGenProgressBar==0,"terminal programmatic close clears stale handles");
 GenerateRandomizerSeed();tickUntil([]{return seedGenStatus.load()==SeedGenerateStatus::Ready;});live=0;UpdateSeedGenerationDialog();require(seedGenDialog==0,"ready programmatic close clears stale handles");
 pushFails=true;GenerateRandomizerSeed();require(seedGenStatus==SeedGenerateStatus::Ready&&seedGenDialog==0,"push failure leaves retry available");pushFails=false;
 blocked=true;GenerateRandomizerSeed();tickUntil([]{return active.load()==1;});UpdateProgressBar();CancelSeedGeneration();WaitForSeedGeneration();require(active==0,"shutdown drains cancelled worker before unloading UI");UpdateSeedGenerationDialog();
 std::cout<<"PASS .741 Back/cancel/retry, one worker, stale callbacks, failure/exception recovery, terminal close, push failure, UI thread and shutdown\n";return 0;
 }catch(const std::exception& e){blocked=false;std::cerr<<e.what()<<'\n';
'''+('return 1;' if a.negative_control else 'CancelSeedGeneration();WaitForSeedGeneration();return 1;')+r'''}}
'''
 if a.negative_control:
  ui_driver=ui_driver.replace(' GenerateRandomizerSeed();auto old=live;', ' buildDialog();require(last.on_dismiss!=nullptr,\"MISSING_BACK_DISMISS_CALLBACK\"); GenerateRandomizerSeed();auto old=live;')
  # New APIs do not exist in .740; the shared Back assertion executes first.
  ui_driver=ui_driver.replace('namespace randomizer {bool GenerateAndWriteSeed()', 'namespace randomizer::ui {bool IsSeedGenerationCancelled(){return false;}void CancelSeedGeneration(){}void WaitForSeedGeneration(){}}\nnamespace randomizer {bool GenerateAndWriteSeed()')
 run('generation_ui',ui_driver,'MISSING_BACK_DISMISS_CALLBACK' if a.negative_control else None)
 # Real retry/write policy with controlled generator and file-writer results.
 policy=common+r'''
namespace randomizer::ui {bool cancelled=false;int rerolls=0;std::string message;bool IsSeedGenerationCancelled(){return cancelled;}void NewRandomSeed(){++rerolls;}void UpdateGenerationStatusMsg(const std::string& s){message=s;}}
namespace fmt {template<class... T>std::string format(const char* s,const T&...){return s;}}
int attempts=0,cleanups=0,writes=0;bool fail=false,plando=false,cancelDuring=false,cancelWrite=false;
namespace randomizer {namespace paths{std::string GetRandomizerPath(){return "test";}}struct Config{bool IsUsingPlandomizer(){return plando;}std::string GetHash(){return "hash";}};struct Randomizer{Randomizer(std::string){}Config c;Config& GetConfig(){return c;}int GetWorld(){return 0;}std::optional<std::string> Generate(){++attempts;if(cancelDuring)ui::cancelled=true;return fail?std::optional<std::string>("invalid"):std::nullopt;}};}
void DeleteFailedGenerationFiles(randomizer::Randomizer&){++cleanups;}
struct RandomizerContext{std::string mHash;std::optional<std::string> WriteToFile(){++writes;return {};}std::string FormatSeedAuditReport(){return "verified";}};
RandomizerContext WriteSeedData(int){if(cancelWrite)randomizer::ui::cancelled=true;return {};}
std::mutex sLastGeneratedSeedMutex;std::string sLastGeneratedSeedHash;
'''+function(read('mods/randomizer/src/randomizer_context.cpp'),'bool GenerateAndWriteSeed()')+r'''
int main(){try{fail=true;require(!GenerateAndWriteSeed()&&attempts==50&&randomizer::ui::rerolls==49&&writes==0,"50 failed attempts remain bounded");attempts=0;plando=true;require(!GenerateAndWriteSeed()&&attempts==1,"plandomizer fails once");plando=false;fail=false;attempts=0;randomizer::ui::cancelled=true;require(!GenerateAndWriteSeed()&&attempts==0,"cancel starts no new attempt");randomizer::ui::cancelled=false;cancelDuring=true;require(!GenerateAndWriteSeed()&&writes==0,"cancelled generation does not write");cancelDuring=false;randomizer::ui::cancelled=false;cancelWrite=true;require(!GenerateAndWriteSeed()&&writes==0,"cancelled seed serialization does not publish");cancelWrite=false;randomizer::ui::cancelled=false;require(GenerateAndWriteSeed()&&writes==1&&sLastGeneratedSeedHash=="hash","verified success still publishes");std::cout<<"PASS .741 real 50-attempt policy, single plando attempt, cancelled seed cleanup and verified publication\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
'''
 if not a.negative_control:run('generation_policy',policy)
 reset=common+r'''
#define _UWP 1
#define TARGET_PC 1
#define PLATFORM_WII 0
#define VERSION 1
#define VERSION_SHIELD_DEBUG 99
using s16=int16_t;using BOOL=int;struct scene_class{};bool pending=false,cardIdle=true,accept=true;int requests=0,reRequests=0,fade=-1,frames=-1,syncs=0,bgmStops=0,audioResets=0;
bool tpr_xbox_reset_to_launcher_pending(){return pending;}
struct mDoRst{inline static bool reset=true;static bool isReset(){return reset;}static bool isReturnToMenu(){return false;}};
namespace JUtility{struct TColor{int rgba;};}JUtility::TColor g_blackColor{};
struct Fader{int status=2;int getStatus(){return status;}};
struct mDoGph_gInf_c{inline static Fader fader;static Fader* getFader(){return &fader;}static void setFadeColor(JUtility::TColor){}};
void dComIfGp_offEnableNextStage(){}void dComIfGp_setNextStage(const char*,int,int,int){}const char* dComIfGp_getNextStageName(){return "opening";}int dComIfGp_getNextStageRoomNo(){return 0;}int dComIfGp_getNextStageLayer(){return 0;}void mDoAud_setSceneName(const char*,int,int){}void dComIfGs_setRestartRoomParam(int){}
int fopScnM_ChangeReq(scene_class*,s16,int f,int n){++requests;fade=f;frames=n;return accept?1:0;}void fopScnM_ReRequest(s16,int){++reRequests;}
constexpr s16 fpcNm_OPENING_SCENE_e=1;bool mDoMemCd_isCardCommNone(){return cardIdle;}struct MemCard{void SaveSync(){++syncs;}}g_mDoMemCd_control;void mDoAud_bgmStop(int){++bgmStops;}void mDoAud_resetProcess(){++audioResets;}
'''+function(read('src/d/d_com_inf_game.cpp'),'int dComIfG_changeOpeningScene(')+'\n'+function(read('src/d/d_com_inf_game.cpp'),'BOOL dComIfG_resetToOpening(')+r'''
int main(){try{scene_class scene;require(!dComIfG_resetToOpening(&scene)&&requests==0,"ordinary fade Wait still blocks");pending=true;cardIdle=false;require(!dComIfG_resetToOpening(&scene)&&requests==0&&syncs==0,"pending save must remain guarded");cardIdle=true;require(dComIfG_resetToOpening(&scene)&&requests==1&&fade==0x7fff&&frames==0,"BLACK_FADER_BLOCKS_RESET_REQUEST");require(syncs==1&&bgmStops==1&&audioResets==1&&reRequests==1,"native save/audio reset retained");accept=false;require(!dComIfG_resetToOpening(&scene)&&bgmStops==1&&audioResets==1&&reRequests==1,"rejected scene request must not advance teardown");pending=false;accept=true;mDoGph_gInf_c::fader.status=0;require(dComIfG_resetToOpening(&scene)&&fade==0&&frames==30,"ordinary opening fade retained");std::cout<<"PASS .741 black-fader no-fade reset, busy save, rejected scene request and ordinary fade preservation\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
'''
 run('reset_request',reset,'BLACK_FADER_BLOCKS_RESET_REQUEST' if a.negative_control else None)
 card=common+r'''
using s32=int32_t;enum CARDFileType{CARD_RAWIMAGE,CARD_GCIFOLDER};CARDFileType SelectedFileType=CARD_GCIFOLDER;bool Initialized=false;std::array<std::filesystem::path,2>cardPaths;
struct Logger{template<class...T>void info(const char*,const T&...){}template<class...T>void warn(const char*,const T&...){}template<class...T>void fatal(const char*,const T&...){throw std::runtime_error("fatal");}}Log;
namespace aurora{const char* g_gameName="GZ2E";struct Config{const char* userPath=nullptr;}g_config;namespace io{std::filesystem::path fs_path_from_string(const char* p){return std::filesystem::u8path(p);}std::string fs_path_to_string(const std::filesystem::path& p){return p.string();}}namespace card{enum class ECardSlot{SlotA,SlotB};
int formats=0;struct CardGciFolder{std::filesystem::path path;void InitCard(const char*,const char*){}bool open(const std::filesystem::path& p){path=p;return std::filesystem::is_directory(p);}bool format(ECardSlot){++formats;std::filesystem::create_directories(path);return true;}void close(){}};struct CardRawFile:CardGciFolder{};}}
std::array<std::unique_ptr<aurora::card::CardGciFolder>,2>CardChannels;
CARDFileType selected_card_type(){return SelectedFileType;}
std::filesystem::path get_card_full_path(std::filesystem::path p,const char*,CARDFileType,aurora::card::ECardSlot slot){return p/(slot==aurora::card::ECardSlot::SlotA?"A":"B");}
'''+function(read('extern/aurora/lib/dolphin/card.cpp'),'void CARDSetBasePath(')+'\n'+function(read('extern/aurora/lib/dolphin/card.cpp'),'void CARDInit(')+r'''
void fresh(const std::filesystem::path& path){Initialized=false;cardPaths={};aurora::card::formats=0;std::filesystem::create_directories(path);auto utf8=path.u8string();CARDSetBasePath(reinterpret_cast<const char*>(utf8.c_str()),-1);CARDInit("GZ2E","01");}
int main(int argc,char**argv){try{require(argc==2,"workdir");std::filesystem::path p=argv[1];std::filesystem::create_directories(p/"onlyB/B");{std::ofstream o(p/"onlyB/B/save.gci");o<<"KEEP_B";}fresh(p/"onlyB");require(std::filesystem::is_directory(p/"onlyB/A")&&aurora::card::formats==1,"SLOT_B_PREVENTS_MISSING_SLOT_A_CREATION");std::string bytes;{std::ifstream f(p/"onlyB/B/save.gci");f>>bytes;}require(bytes=="KEEP_B","existing B retained");fresh(p/"new");require(aurora::card::formats==1,"fresh install supplies Slot A");fresh(p/"new");require(aurora::card::formats==0,"existing Slot A never formatted");std::filesystem::create_directories(p/"bad");{std::ofstream o(p/"bad/A");o<<"BAD_CARD_PRESERVE";}fresh(p/"bad");require(aurora::card::formats==0&&std::filesystem::is_regular_file(p/"bad/A"),"failed existing card never formatted");fresh(p/std::filesystem::path(u8"saves_雪"));require(std::filesystem::is_directory(p/std::filesystem::path(u8"saves_雪/A")),"UTF-8 save root round trip");std::cout<<"PASS .741 actual CARDInit missing A with existing B, fresh install, existing/bad card preservation and UTF-8 paths\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
'''
 run('vanilla_card',card,'SLOT_B_PREVENTS_MISSING_SLOT_A_CREATION' if a.negative_control else None)
 if not a.negative_control:
  ui_loop=read('mods/randomizer/src/ui/ui.cpp')
  assert '#if !defined(_UWP)\nstatic std::atomic uiRunning' in ui_loop
  assert 'UpdateProgressBar();' in function(ui_loop,'void update()')
  assert 'WaitForSeedGeneration();' in function(ui_loop,'ModResult shutdown()')
  assert 'const auto preferredKind = save_manager::StorageKind::GciDirectory;' in read('src/dusk/ui/saves_window.cpp')
  print('PASS .741 UWP UI-thread updates and vanilla launcher save backend match; desktop progress path retained')
