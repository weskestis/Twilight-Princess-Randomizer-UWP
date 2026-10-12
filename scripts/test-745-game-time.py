"""Exercise production game-time storage and frame counting with a save backend fixture."""
import argparse,os,re,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('source',type=Path);a=p.parse_args();r=a.source.resolve()
source=(r/'mods/randomizer/src/randomizer_context.cpp').read_text()
def function(name):
 m=re.search(r'(?m)^(?:[\w:<>, ]+)\b'+re.escape(name)+r'\([^;{}]*\)(?: noexcept)?\s*\{',source)
 assert m,name
 start=m.end()-1;depth=1;end=start+1
 while depth:depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[m.start():end]+'\n'
constants=source[source.index('constexpr std::string_view kActiveGameTimeBlobName'):source.index('std::mutex sEntranceDiscoveryMutex')]
globals=source[source.index('std::mutex sActiveGameTimeMutex'):source.index('std::mutex sLastGeneratedSeedMutex')]
production=constants+globals+''.join(function(n) for n in ['EntranceSeedFingerprint','AppendU16','AppendU32','AppendU64','ReadU16','ReadU32','ReadU64','NativeGameTimeFrameBaseline','PersistActiveGameTimeLocked','LoadActiveGameTimeLocked','EnsureActiveGameTimeLocked','randomizer_AdvanceActiveGameTimeFrame','randomizer_GetActiveGameTimeCentiseconds'])
match=re.search(r'randomizer_AdvanceActiveGameTimeFrame\(\s*(randomizer_IsActive\(\).*?)\);',source,re.S);assert match
predicate='bool active_frame() { return '+match.group(1)+'; }\n'
prefix=r'''
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <exception>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
using u8=uint8_t;using u16=uint16_t;using u32=uint32_t;using u64=uint64_t;using s32=int32_t;using s64=int64_t;
struct Context { std::string mHash="seed-a"; } context;
Context& randomizer_GetContext() { return context; }
bool currentReloadingState=false,active=true,paused=false;unsigned gameover=0;
bool randomizer_IsActive(){return active;}bool dComIfGp_isPauseFlag(){return paused;}
unsigned dComIfGp_getGameoverStatus(){return gameover;}
s64 nativeTotal=0,wall=0;constexpr s64 OS_TIMER_CLOCK=1000;
s64 dComIfGs_getSaveStartTime(){return 0;}s64 OSGetTime(){return wall;}
s64 dComIfGs_getSaveTotalTime(){return nativeTotal;}
namespace mods::log { template<class...T>void error(T...){ }template<class...T>void warn(T...){ } }
namespace randomizer::session {
u64 revision=1;int slot=0;unsigned writes=0;
std::map<std::string,std::vector<u8>> blobs;
std::string key(){return std::to_string(slot)+":"+context.mHash;}
u64 saveStateRevision(){return revision;}int currentSaveSlot(){return slot;}
bool setSaveBlob(std::string_view,const void* data,size_t size){const auto* b=static_cast<const u8*>(data);blobs[key()]={b,b+size};++writes;return true;}
std::optional<std::vector<u8>> getSaveBlob(std::string_view){auto i=blobs.find(key());return i==blobs.end()?std::nullopt:std::optional(i->second);}
}
'''
tests=r'''
int main() {
 using namespace randomizer::session;
 assert(randomizer_GetActiveGameTimeCentiseconds()==0);
 for(unsigned i=0;i<30;++i)randomizer_AdvanceActiveGameTimeFrame(active_frame());
 assert(randomizer_GetActiveGameTimeCentiseconds()==100);
 const auto stored=blobs[key()];assert(stored.size()==24);
 const auto count=writes;
 for(unsigned render=0;render<1200;++render){wall+=1000;assert(randomizer_GetActiveGameTimeCentiseconds()==100);}
 assert(writes==count);
 for(unsigned reason=0;reason<4;++reason){
  paused=reason==0;currentReloadingState=reason==1;gameover=reason==2?1:0;active=reason!=3;
  for(unsigned i=0;i<300;++i)randomizer_AdvanceActiveGameTimeFrame(active_frame());
  assert(randomizer_GetActiveGameTimeCentiseconds()==100);
 }
 paused=false;currentReloadingState=false;gameover=0;active=true;
 context.mHash="seed-b";++revision;assert(randomizer_GetActiveGameTimeCentiseconds()==0);
 for(unsigned i=0;i<60;++i)randomizer_AdvanceActiveGameTimeFrame(active_frame());
 assert(randomizer_GetActiveGameTimeCentiseconds()==200);
 context.mHash="seed-a";++revision;assert(randomizer_GetActiveGameTimeCentiseconds()==100);
 slot=1;++revision;assert(randomizer_GetActiveGameTimeCentiseconds()==0);
 slot=0;++revision;assert(randomizer_GetActiveGameTimeCentiseconds()==100);
 blobs[key()][8]^=1;nativeTotal=2500;++revision;
 assert(randomizer_GetActiveGameTimeCentiseconds()==250);
 blobs[key()]={1,2,3};++revision;assert(randomizer_GetActiveGameTimeCentiseconds()==250);
 slot=-1;++revision;assert(randomizer_GetActiveGameTimeCentiseconds()==0);
 randomizer_AdvanceActiveGameTimeFrame(true);assert(randomizer_GetActiveGameTimeCentiseconds()==0);
 puts("PASS: production 30 Hz IGT, hundredths, pause/load/game-over/inactive exclusion, render-rate/wall-clock independence, save/seed/slot persistence and corrupt blob handling");
}
'''
with tempfile.TemporaryDirectory(prefix='game-time-745-') as tmp:
 w=Path(tmp);f=w/'time.cpp';f.write_text(prefix+production+predicate+tests);exe=w/('time.exe' if os.name=='nt' else 'time')
 cmd=['cl','/nologo','/EHsc','/std:c++20',str(f),'/Fe:'+str(exe)] if os.name=='nt' else ['c++','-std=c++20','-O1',str(f),'-o',str(exe)]
 subprocess.run(cmd,cwd=w,check=True);subprocess.run([str(exe)],check=True)
