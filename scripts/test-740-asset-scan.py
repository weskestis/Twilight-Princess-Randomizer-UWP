"""Compile production bundle/overlay/texture functions, including the shipped negative control."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

p=argparse.ArgumentParser()
p.add_argument('source',type=Path)
p.add_argument('--negative-control',action='store_true')
a=p.parse_args();root=a.source.resolve();control=Path(__file__).resolve().parents[1]

with tempfile.TemporaryDirectory(prefix='tpr-740-assets-') as temporary:
 work=Path(temporary);scanroot=root
 paths=['src/dusk/mods/loader/loader.hpp','src/dusk/mods/loader/bundle_disk.cpp',
        'src/dusk/mods/svc/overlay.cpp','src/dusk/mods/svc/texture.cpp']
 if a.negative_control:
  scanroot=work/'old';scanroot.mkdir()
  for name in paths:
   out=scanroot/name;out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes((root/name).read_bytes())
  subprocess.run(['git','-C',str(scanroot),'apply','--reverse',str(control/'patches/xbox-asset-scan-740-host.patch')],check=True)
 header=(scanroot/paths[0]).read_text();disk=(scanroot/paths[1]).read_text()
 overlay=(scanroot/paths[2]).read_text();texture=(scanroot/paths[3]).read_text()
 base=header[header.index('class ModBundle {'):header.index('class ModBundleZip')]
 diskclass=header[header.index('class ModBundleDisk'):header.index('LoadedMod* mod_from_context')]
 diskbody=disk[disk.index('ModBundleDisk::ModBundleDisk'):disk.rindex('}  // namespace dusk::mods')]
 overlaybody=overlay[overlay.index('struct OverlayFileData'):overlay.index('uint64_t overlay_add_file(')]
 internal=(root/'src/dusk/mods/svc/internal.hpp').read_text()
 slot=internal[internal.index('template <typename T>\nclass SlotMap'):internal.index('\n};',internal.index('class SlotMap'))+3]
 texturesync=texture[texture.index('void textures_sync_replacements() {'):texture.index('void textures_lifecycle_applied()')]
 driver=r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
using u8=uint8_t;
using namespace std::string_literals;
namespace fs=std::filesystem;
fs::path ConfigPath;
struct TestLog {template<class... T> void warn(const char*,const T&...){} template<class... T> void error(const char*,const T&...){}
 template<class... T> void debug(const char*,const T&...){} template<class... T> void info(const char*,const T&...){}
 template<class... T> void fatal(const char*,const T&...){throw std::runtime_error("fatal");}};
TestLog Log;
namespace dusk {bool IsGameLaunched=false;}
namespace startup_guard {bool pending=false;std::string last;void stage(std::string_view s){last=s;}bool activity_pending(){return pending;}}
namespace xbox_memory {std::string failure;int errors=0;
 void record(const fs::path&,std::string_view operation,std::string_view id,std::string_view={},std::string_view error={}) {
  if(!error.empty()){failure=std::string(operation)+" "+std::string(id)+" "+std::string(error);++errors;}}
 void record_path(const fs::path&,std::string_view,const fs::path&){} }
namespace borealis::io {std::string fs_path_to_string(const fs::path& p){auto s=p.u8string();return {reinterpret_cast<const char*>(s.data()),s.size()};}}
namespace io {struct FileStream {static std::vector<u8> ReadAllBytes(const fs::path& p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("read");return {std::istreambuf_iterator<char>(f),{}};}};}
BASE
DISKCLASS
DISKBODY
struct Metadata {std::string id,version="1.0.0";};
struct LoadedMod {Metadata metadata;std::shared_ptr<ModBundle> bundle;bool active=true;};
struct ModLoader {std::vector<LoadedMod> all;static ModLoader& instance(){static ModLoader loader;return loader;}
 std::vector<LoadedMod>& active_mods(){return all;}};
SLOTMAP
struct AuroraOverlayFile {const char* fileName;void* userdata;size_t size;};
struct AuroraOverlayCallbacks {void*(*open)(void*);void(*close)(void*);int64_t(*read)(void*,uint8_t*,size_t);int64_t(*seek)(void*,int64_t,int32_t);};
struct Published {std::string path;void* userdata;size_t size;};
std::vector<Published> published;int publishes=0,callbackSets=0,archiveNotifies=0;
const AuroraOverlayCallbacks* callbacks=nullptr;
void aurora_dvd_overlay_callbacks(const AuroraOverlayCallbacks* c){callbacks=c;++callbackSets;}
void aurora_dvd_overlay_files(const AuroraOverlayFile* files,size_t size,void*){++publishes;published.clear();for(size_t i=0;i<size;++i)published.push_back({files[i].fileName,files[i].userdata,files[i].size});}
struct JKRArchive {static void notifyOverlayFilesChanged(){++archiveNotifies;}};
namespace overlay_fixture {
OVERLAY
}
struct Bundle:ModBundle {
 std::vector<std::string> names;std::unordered_map<std::string,std::vector<u8>> bytes;
 bool failNames=false,failSize=false;int fullScans=0,scopedScans=0;
 std::vector<std::string> getFileNames()override{++fullScans;if(failNames)throw fs::filesystem_error("injected full scan",fs::path("package"),std::make_error_code(std::errc::permission_denied));return names;}
 std::vector<std::string> getFileNamesUnder(std::string_view prefix) SCOPED_OVERRIDE {++scopedScans;if(failNames)throw fs::filesystem_error("injected scoped scan",fs::path("package")/prefix,std::make_error_code(std::errc::permission_denied));std::vector<std::string> f;for(const auto& n:names)if(n.starts_with(prefix))f.push_back(n);return f;}
 std::vector<u8> readFile(const std::string& n)override{return bytes.at(n);}
 size_t getFileSize(const std::string& n)override{if(failSize)throw fs::filesystem_error("injected size query",fs::path(n),std::make_error_code(std::errc::io_error));return bytes.at(n).size();}
 bool file_exists(const std::string& n)override{return bytes.contains(n);}
 bool directory_exists(const std::string&)override{return false;}
};
namespace aurora::texture {
 struct ReplacementGroup {std::vector<int> registrations;};
 void unregister_replacements(ReplacementGroup& g){g.registrations.clear();}
 void unregister_replacement(int& r){r=0;}
}
namespace texture_fixture {
struct RuntimeEntry {int registration=77;};
struct ModTextureRecord {int32_t appliedPriority=0;bool staticRegistered=false;aurora::texture::ReplacementGroup staticGroup;std::vector<int> staticKeepalives;std::vector<RuntimeEntry> runtime;};
std::unordered_map<const LoadedMod*,ModTextureRecord> s_modTextures;
int32_t currentPriority=1;int registrations=0,runtimeRegistrations=0;
int32_t compute_mod_priority(const LoadedMod&){return currentPriority;}
void unregister_record(ModTextureRecord& r){r.staticGroup.registrations.clear();r.staticKeepalives.clear();}
void register_runtime_entry(RuntimeEntry& e,int32_t p){e.registration=p;++runtimeRegistrations;}
void register_static_textures(LoadedMod& mod,ModTextureRecord& record FILES_PARAMETER){
 FILES_BODY
 for(const auto& file:files)if(file.starts_with("textures/")){record.staticGroup.registrations.push_back(++registrations);record.staticKeepalives.push_back(registrations);}
 record.staticRegistered=true;
}
TEXTURESYNC
}
void write(const fs::path& p,std::string_view content){fs::create_directories(p.parent_path());std::ofstream o(p,std::ios::binary);o<<content;}
void require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
int main(int argc,char** argv){try{
 require(argc==2||argc==3,"workdir");fs::path p=argv[1];fs::create_directories(p);
 write(p/"mod.json","{}");write(p/"res/licenses/LUAU-MIT.txt","license");write(p/"res/licenses/LUABRIDGE3-MIT.txt","license");
 ModBundleDisk disk(p);
 DISK_ASSERTIONS
 write(p/"overlay/room$palette.bin","123");write(p/fs::path(u8"overlay/雪.bin"),"4567");write(p/"overlay/nested/object.bin","89");write(p/"textures/deep/asset.png","data");
 DISK_ASSET_ASSERTIONS
 auto luau=std::make_shared<Bundle>();luau->names={"mod.json","res/licenses/LUAU-MIT.txt","res/licenses/LUABRIDGE3-MIT.txt"};
 if(argc==3)luau->failNames=true;
 auto& loader=ModLoader::instance();loader.all.reserve(4);loader.all.push_back({{"dev.twilitrealm.luau"},luau});
 using namespace overlay_fixture;s_overlaysDirty=true;
 for(int i=0;i<120;++i)if(s_overlaysDirty)overlay_sync_files();
 require(luau->scopedScans==0,"prelaunch scan must defer");dusk::IsGameLaunched=true;
 for(int i=0;i<120;++i)if(s_overlaysDirty)overlay_sync_files();
 require(!s_overlaysDirty,"dirty flag must clear after stable scan");require(luau->fullScans==0 && luau->scopedScans==1,"Luau must scan only overlay once");
 require(callbackSets==0 && publishes==0,"assetless Luau must not register DVD callbacks");
 for(int i=0;i<1000;++i)if(s_overlaysDirty)overlay_sync_files();require(luau->scopedScans==1,"idle frames must not re-enumerate");
 auto runtime=std::make_shared<const std::vector<u8>>(std::vector<u8>{9,8,7});
 s_runtimeOverlays.emplace(loader.all[0],RuntimeOverlaySlot{.discPath="/runtime.bin",.buffer=runtime,.size=3,.order=1});
 s_overlaysDirty=true;overlay_sync_files();require(published.size()==1&&published[0].path=="/runtime.bin","assetless runtime overlay retained");
 auto* open=callbacks->open(published[0].userdata);require(open,"runtime open");uint8_t read[8]{};require(callbacks->read(open,read,8)==3&&read[0]==9,"runtime bytes");
 auto assets=std::make_shared<Bundle>();assets->names={"overlay/room.bin","textures/asset.png","res/not-an-overlay.bin"};assets->bytes["overlay/room.bin"]={1,2};
 loader.all.push_back({{"optional.assets"},assets});s_overlaysDirty=true;overlay_sync_files();require(published.size()==2,"static and runtime overlays");
 const auto prior=published;const auto count=publishes;const auto size=s_overlayFiles.size();
 assets->failNames=true;s_overlaysDirty=true;overlay_sync_files();require(publishes==count&&s_overlayFiles.size()==size,"failed scan must keep registry");
 require(xbox_memory::failure.find("optional.assets")!=std::string::npos,"failure identifies package");require(!s_overlaysDirty,"failure must not retry every frame");
 require(callbacks->read(open,read,8)==0,"open handle survives failed replacement");callbacks->close(open);
 for(const auto& f:prior){open=callbacks->open(f.userdata);require(open,"published ID survives failed replacement");callbacks->close(open);}
 assets->failNames=false;assets->failSize=true;s_overlaysDirty=true;overlay_sync_files();require(publishes==count&&s_overlayFiles.size()==size,"failed size must keep registry");
 assets->failSize=false;s_overlaysDirty=true;overlay_sync_files();require(publishes==count+1,"successful retry publishes once");
 s_runtimeOverlays.erase_all(loader.all[0]);loader.all.clear();s_overlaysDirty=true;overlay_sync_files();require(published.empty()&&s_overlayFiles.empty(),"disable clears old overlays");
 require(archiveNotifies==publishes,"archive notifications follow successful publishes only");
 loader.all.push_back({{"optional.texture"},assets});
 using namespace texture_fixture;
 textures_sync_replacements();auto& record=s_modTextures[&loader.all[0]];require(record.staticRegistered&&record.staticGroup.registrations.size()==1,"scoped texture registration");
 const auto group=record.staticGroup.registrations;record.runtime.push_back({77});currentPriority=2;assets->failNames=true;
 textures_sync_replacements();require(record.staticGroup.registrations==group&&record.appliedPriority==1&&record.runtime[0].registration==77,"texture failure preserves live static and runtime entries");
 assets->failNames=false;textures_sync_replacements();require(record.appliedPriority==2&&record.runtime[0].registration==2,"texture reorder recovers runtime priority");
 require(assets->fullScans==0,"asset services never enumerate unrelated package files");
 std::cout<<"PASS .740 production scoped/Unicode/dollar-name bundle scans, idle dirty flag, runtime overlays, failed listing/size rollback, open handles, disable/retry and texture priority preservation\n";
 return 0;
 }catch(const std::exception& e){std::cerr<<"REGRESSION: "<<e.what()<<'\n';return 1;}}
'''
 replacements={'BASE':base,'DISKCLASS':diskclass,'DISKBODY':diskbody,'SLOTMAP':slot,'OVERLAY':overlaybody,'TEXTURESYNC':texturesync,
 'SCOPED_OVERRIDE':'override' if not a.negative_control else '',
 'FILES_PARAMETER':', const std::vector<std::string>& files' if not a.negative_control else '',
 'FILES_BODY':'' if not a.negative_control else 'const auto files=mod.bundle->getFileNames();',
 'DISK_ASSERTIONS':r'''require(disk.getFileNamesUnder("overlay/").empty()&&disk.getFileNamesUnder("textures/").empty(),"Luau has no assets");bool rejected=false;try{disk.getFileNamesUnder("../");}catch(const std::invalid_argument&){rejected=true;}require(rejected,"subtree traversal rejected");''' if not a.negative_control else '',
 'DISK_ASSET_ASSERTIONS':r'''auto names=disk.getFileNamesUnder("overlay/");std::ranges::sort(names);require(names.size()==3,"all nested overlay files");require(std::ranges::find(names,"overlay/room$palette.bin")!=names.end(),"dollar filename preserved");require(std::ranges::find(names,std::string(reinterpret_cast<const char*>(u8"overlay/雪.bin")))!=names.end(),"Unicode filename preserved");auto textures=disk.getFileNamesUnder("textures/");require(textures.size()==1&&textures[0]=="textures/deep/asset.png","texture subtree");require(disk.getFileNames().size()==7,"full inventory remains available");std::error_code linkError;fs::create_directory_symlink(p/"overlay",p/"overlay/cycle",linkError);if(!linkError){require(disk.getFileNamesUnder("overlay/").size()==3,"UWP reparse cycle is not followed");fs::remove(p/"overlay/cycle");}fs::create_symlink(p/"absent",p/"overlay/dangling",linkError);if(!linkError){bool failed=false;try{disk.getFileNamesUnder("overlay/");}catch(const fs::filesystem_error&){failed=true;}require(failed,"status failure must not return partial listing");fs::remove(p/"overlay/dangling");}''' if not a.negative_control else ''}
 for key,value in replacements.items():driver=driver.replace(key,value)
 source=work/'native.cpp';source.write_text(driver,encoding='utf-8');compiler=shutil.which('cl' if os.name=='nt' else 'g++');exe=work/('native.exe' if os.name=='nt' else 'native')
 if not compiler:raise RuntimeError('C++ compiler unavailable')
 command=([compiler,'/nologo','/std:c++20','/EHsc','/utf-8','/D_UWP=1',str(source),'/Fe:'+str(exe)] if os.name=='nt' else [compiler,'-std=c++20','-D_UWP=1','-O1','-g',str(source),'-o',str(exe)])
 subprocess.run(command,check=True,cwd=work)
 result=subprocess.run([str(exe),str(work/'package')],cwd=work,capture_output=True,text=True)
 print(result.stdout,end='');print(result.stderr,end='')
 if a.negative_control:
  if result.returncode==0:raise RuntimeError('Shipped .739 negative control unexpectedly passed')
  if 'dirty flag must clear' not in result.stderr:raise RuntimeError('Negative control failed for an unexpected reason')
  errors=subprocess.run([str(exe),str(work/'fault-package'),'fault'],cwd=work,capture_output=True,text=True)
  if errors.returncode==0 or 'injected full scan' not in errors.stderr:raise RuntimeError('Negative control failed to reproduce uncaught listing exception')
  print('PASS .740 negative control reproduces shipped .739 repeated dirty scans and uncaught bundle listing failures')
 elif result.returncode:raise RuntimeError('Production asset scan regression')
