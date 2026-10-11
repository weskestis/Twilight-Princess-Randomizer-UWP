#include <array>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <utility>
#include "dusk/xbox_vanilla_save.hpp"
using u8=uint8_t;using u16=uint16_t;using u32=uint32_t;using u64=uint64_t;using s32=int32_t;using BOOL=int;
#define FALSE 0
#define TRUE 1
#define SLOT_A 0
#define SAVEDATA_SIZE 0xA94
#define SAVEFILE_SIZE (SAVEDATA_SIZE*3)
#define SECTOR_SIZE 0x2000
#define CARD_FILE_SIZE 0x8000
#define SAVEDATA_VERSION 6
#define OS_REPORT(...) ((void)0)
template<class T>struct BigEndian {u8 bytes[sizeof(T)];operator T()const{T v=0;for(u8 b:bytes)v=(v<<8)|b;return v;}BigEndian&operator=(T v){for(size_t i=0;i<sizeof(T);++i)bytes[sizeof(T)-1-i]=u8(v>>(i*8));return *this;}};
#define BE(T) BigEndian<T>
namespace dusk {
std::filesystem::path ConfigPath;bool IsShuttingDown=false;
namespace version {struct Disc {char gameName[4]={'G','Z','2','E'},company[2]={'0','1'};}disc;const Disc&getDiskID(){return disc;}}
namespace startup_guard {template<class T>void stage(const T&){}}
struct Settings{struct{struct{int getValue(){return 1;}}cardFileType;}backend;};Settings&getSettings(){static Settings s;return s;}
}
namespace fmt {template<class...A>std::string format(const char*s,A&&...){return s;}}
namespace mDoRst {bool resetting=false;bool isReset(){return resetting;}}
struct OSMutex{};struct OSCond{};struct OSThread{};
void OSLockMutex(OSMutex*){}void OSUnlockMutex(OSMutex*){}bool OSTryLockMutex(OSMutex*){return true;}void OSWaitCond(OSCond*,OSMutex*){throw std::runtime_error("direct save waited for CARD worker");}void OSSignalCond(OSCond*){}void OSInitMutex(OSMutex*){}void OSInitCond(OSCond*){}
void OSCreateThread(OSThread*,void*(*)(void*),void*,void*,size_t,int,int){}void OSResumeThread(OSThread*){}int OSGetThreadPriority(OSThread*){return 1;}OSThread*OSGetCurrentThread(){static OSThread t;return &t;}
static OSThread MemCardThread;static u8 MemCardStack[64];int mDoMemCd_main(void*){return 0;}
enum CARDFileType{CARD_GCIFOLDER};void CARDSetLoadType(CARDFileType){}void CARDSetBasePath(const char*,int){}void CARDInit(const char*,const char*){}
int cardProbes=0;BOOL CARDProbe(int){++cardProbes;return FALSE;}s32 CARDProbeEx(int,s32*size,s32*sector){++cardProbes;*size=*sector=0;return -3;}s32 CARDGetResultCode(int){return -3;}
enum {CARD_STATE_NO_CARD_e,CARD_STATE_READY_e,CARD_STATE_NO_FILE_e,CARD_STATE_READ_e,CARD_STATE_WRITE_e,CARD_STATE_FORMAT_e,CARD_STATE_WRONG_ENCODING_e,CARD_STATE_BROKEN_e,CARD_STATE_INSSPACE_e,CARD_STATE_NOENT_e,CARD_STATE_WRONG_DEVICE_e,CARD_STATE_WRONG_SECTORSIZE_e,CARD_STATE_FATAL_ERROR_e,CARD_STATE_13_e};
enum {COMM_NONE_e,COMM_ATTACH_e,COMM_RESTORE_e,COMM_STORE_e,COMM_DETACH_e,COMM_RESCAN_FILE_e};
struct mDoMemCd_Ctrl_c {
    u8 mData[SAVEFILE_SIZE]{};u8 mChannel=0,mCopyToPos=0,mProbeStat=2;int mCardCommand=COMM_NONE_e,mCardState=CARD_STATE_NO_CARD_e,field_0x1fc8=0;OSMutex mMutex;OSCond mCond;u32 mDataVersion=0;bool mInitialized=false;std::string mFileName;
    void ThdInit();void update();void load();void save(void*,u32,u32);void command_attach();void setFileName(const std::string&);const char*getFileName();int getStatus(int){return mCardState;}
};
struct mDoMemCdRWm_HeaderData{u8 data[0x2440];};void mDoMemCdRWm_BuildHeader(mDoMemCdRWm_HeaderData*h){std::memset(h,0x66,sizeof(*h));}
mDoMemCd_Ctrl_c g_mDoMemCd_control;
int remountCalls=0;bool remountResult=false;
bool aurora_card_remount(int){++remountCalls;return remountResult;}
namespace dusk::mods::svc {std::vector<std::string> invalidated;void invalidate_save(std::string_view name){invalidated.emplace_back(name);}}
#include "production-vanilla.inc"
void require(bool v,const char*s){if(!v)throw std::runtime_error(s);}
std::vector<u8> bytes(const std::filesystem::path&p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void write(const std::filesystem::path&p,const std::vector<u8>&b){std::ofstream out(p,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(b.data()),b.size());}
int main(int argc,char**argv) {try{
    require(argc>1,"data path missing");dusk::ConfigPath=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(dusk::ConfigPath/"USA/Card A");
    std::array<u8,SAVEFILE_SIZE> quest{};for(size_t i=0;i<quest.size();++i)quest[i]=u8(i*17);
    for(u8 i=0;i<3;++i)mDoMemCdRWm_SetCheckSumGameData(quest.data(),i);
    std::vector<u8> packet;mDoMemCdRWm_BuildXboxGci(quest.data(),packet);
    const auto path=dusk::ConfigPath/"USA/Card A/01-GZ2E-gczelda2.gci";write(path,packet);const auto original=bytes(path);
    mDoMemCd_Ctrl_c control;control.ThdInit();
#ifdef TPR_NEGATIVE_CONTROL
    return control.mCardState==CARD_STATE_NO_CARD_e&&control.mCardCommand==COMM_ATTACH_e?42:1;
#else
    using namespace dusk::save_manager;
    g_mDoMemCd_control.ThdInit();g_mDoMemCd_control.mCardState=CARD_STATE_NO_CARD_e;
    Storage mounted{.kind=StorageKind::GciDirectory,.channel=0,.mounted=true};
    std::array<SaveIdentity,1> vanilla{{{"01","GZ2E","gczelda2"}}},rando{{{"01","GZ2E","randomizer-xbox"}}};
    require(finish_write(mounted,vanilla).ok&&remountCalls==0&&g_mDoMemCd_control.mCardState==CARD_STATE_READY_e,"Mounted vanilla import still requires virtual CARD or fails to refresh active state");
    require(dusk::mods::svc::invalidated==std::vector<std::string>{"gczelda2"},"Vanilla import did not invalidate cached save data");
    g_mDoMemCd_control.setFileName("randomizer-xbox");
    require(finish_write(mounted,vanilla).ok&&remountCalls==0&&g_mDoMemCd_control.mCardState==CARD_STATE_NO_FILE_e,"Inactive vanilla import changed the active Randomizer card");
    require(!finish_write(mounted,rando).ok&&remountCalls==1,"Randomizer import bypassed its ordinary remount result");
    mounted.kind=StorageKind::RawImage;
    require(!finish_write(mounted,vanilla).ok&&remountCalls==2,"Raw card import bypassed the virtual card remount");
    mounted.mounted=false;
    require(finish_write(mounted,rando).ok&&remountCalls==2,"Unmounted import unexpectedly remounted a virtual card");
    require(control.mCardState==CARD_STATE_READY_e&&control.mCardCommand==COMM_NONE_e,"Vanilla initialization still depends on CARD worker");
    const auto probes=cardProbes;for(int i=0;i<180;++i)control.update();require(cardProbes==probes&&control.mCardState==CARD_STATE_READY_e,"CARD polling overwrote direct ready state");
    control.load();require(control.mCardState==CARD_STATE_READ_e&&std::memcmp(control.mData,quest.data(),quest.size())==0,"Existing vanilla GCI not read");
    quest[2]^=0x55;mDoMemCdRWm_SetCheckSumGameData(quest.data(),0);control.save(quest.data(),quest.size(),0);
    require(control.mCardState==CARD_STATE_WRITE_e&&std::filesystem::exists(path.string()+".bak"),"Vanilla save failed or previous GCI lost");
    require(bytes(path.string()+".bak")==original,"Backup did not preserve previous save");
    mDoMemCd_Ctrl_c reboot;reboot.ThdInit();reboot.load();require(reboot.mCardState==CARD_STATE_READ_e&&std::memcmp(reboot.mData,quest.data(),quest.size())==0,"Vanilla GCI did not survive restart");
    require(packet.size()==64+0x8000&&packet[7]==5&&packet[56]==0&&packet[57]==4&&packet[62]==0x24,"GCI layout/card metadata changed");
    auto damaged=bytes(path);damaged[64+0x4000+8]^=1;write(path,damaged);reboot.load();require(reboot.mCardState==CARD_STATE_READ_e&&std::memcmp(reboot.mData,quest.data(),quest.size())==0,"Primary quest corruption did not recover valid backup sector");
    // Original-console metadata stores the data version in big-endian form.
    damaged[64+0x4000+4]=0;damaged[64+0x4000+7]=6;damaged[64+0x6000+4]=0;damaged[64+0x6000+7]=6;write(path,damaged);reboot.load();require(reboot.mCardState==CARD_STATE_READ_e,"Console/Dolphin GCI version rejected");
    const auto vanillaBytes=bytes(path);reboot.setFileName("randomizer-xbox");require(reboot.mCardState==CARD_STATE_NO_FILE_e,"Randomizer inherited vanilla save");reboot.save(quest.data(),quest.size(),0);require(std::filesystem::exists(dusk::ConfigPath/"randomizer-xbox.sav")&&bytes(path)==vanillaBytes,"Randomizer save isolation or legacy format changed");
    reboot.setFileName("gczelda2");require(reboot.mCardState==CARD_STATE_READY_e,"Switch to vanilla stayed on worker/no-card state");
    auto bad=vanillaBytes;bad[0]='X';write(path,bad);reboot.command_attach();require(reboot.mCardState==CARD_STATE_FATAL_ERROR_e&&bytes(path)==bad,"Invalid save silently replaced or mistaken for no file");
    write(path,vanillaBytes);std::filesystem::remove(path);reboot.command_attach();require(reboot.mCardState==CARD_STATE_READY_e&&std::filesystem::exists(path),"Interrupted atomic replace did not recover preserved GCI backup");
    const auto beforeFailure=bytes(path);std::filesystem::create_directory(path.string()+".tmp");reboot.save(quest.data(),quest.size(),0);require(reboot.mCardState==CARD_STATE_FATAL_ERROR_e&&bytes(path)==beforeFailure,"Failed write damaged existing GCI");
    std::filesystem::remove(path.string()+".tmp");std::filesystem::remove(path);std::filesystem::remove(path.string()+".bak");reboot.command_attach();require(reboot.mCardState==CARD_STATE_NO_FILE_e&&reboot.mProbeStat==0,"Fresh vanilla has no card instead of no save file");
    puts("PASS: production vanilla initialization/load/save/restart, mounted import refresh, standard GCI backup recovery, corruption preservation and Randomizer isolation");return 0;
#endif
}catch(const std::exception&e){fprintf(stderr,"%s\n",e.what());return 1;}}
