"""Fault-test production package preparation and Xbox budget diagnostics."""
import argparse
import ast
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('source', type=Path)
args = p.parse_args()
root = args.source.resolve()
control = Path(__file__).resolve().parents[1]
loader = (root/'src/dusk/mods/loader/loader.cpp').read_text()
function = loader[loader.index('LoadedMod* ModLoader::try_load_mod('):loader.index('bool ModLoader::activate_mod(')]
natives = (root/'src/dusk/mods/loader/natives.cpp').read_text()
builtin = natives[natives.index('bool load_builtin_randomizer('):natives.index('bool load_builtin_cosmetics(')]
prelaunch = (root/'src/dusk/ui/prelaunch.cpp').read_text()
if 'prelaunch.memory-budget' in prelaunch:
    start = prelaunch.index('            xbox_memory::record(ConfigPath, "prelaunch.memory-budget"')
    gate = prelaunch[start:prelaunch.index('#endif', start)]
else:
    # Local patch construction precedes the full Windows source reconstruction.
    tree = ast.parse((control/'scripts/apply-739-package-startup.py').read_text())
    gate = next(ast.literal_eval(node.value) for node in tree.body
        if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'guard' for t in node.targets))
    gate = gate.replace('#if defined(_UWP)', '').replace('#endif', '')

driver = r'''
#include <cassert>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <mods/api.h>
#include "dusk/xbox_memory_budget.hpp"
#include "dusk/mods/uwp_ports.hpp"
struct ModContext { void* mod=nullptr; };
namespace fs = std::filesystem;
namespace xbox_memory = dusk::xbox_memory;
namespace startup_guard = dusk::startup_guard;
namespace uwp_ports = dusk::mods::uwp_ports;
fs::path ConfigPath;
std::string fault;
xbox_memory::Budget budget{0, 5ull<<30, true, true};
extern "C" bool dusk_query_memory_budget(uint64_t* usage, uint64_t* limit, bool* xbox) noexcept {
    *usage=budget.usage; *limit=budget.limit; *xbox=budget.xbox; return budget.available;
}
namespace fmt { template<class T> std::string format(const char*,const T& text) { return std::string(text); } }
namespace fixture {
struct TestLog { template<class... T> void error(const char*,const T&...) {} } Log;
enum { LOG_LEVEL_INFO, LOG_LEVEL_ERROR };
namespace log { template<class... T> void write(const std::string&,int,const char*,const T&...) {
    if(fault=="log") throw std::runtime_error("injected log failure");
} }
namespace data { std::string abbreviated_path_string(const fs::path& p) { return p.string(); } }
namespace borealis::io { std::string fs_path_to_string(const fs::path& p) {
    if(fault=="path-conversion") throw std::runtime_error("injected path conversion failure");
    return p.string();
} }
template<class T> struct ConfigVar {
    bool value;
    ConfigVar(const std::string&, bool enabled):value(enabled) {
        if(fault=="config") throw std::runtime_error("injected config failure");
    }
};
struct ModBundle {};
struct Metadata { std::string id,name,version,author; };
struct Runtime { std::string id; uint16_t major=0,minMinor=0; };
struct Info { struct Import { std::string id; uint16_t major,minMinor; bool required; }; std::vector<Import> imports; };
struct LoadedMod;
struct NativeMod {
    const ModMeta* meta=nullptr; ModContext** contextSymbol=nullptr;
    ModInitializeFn fn_initialize=nullptr; ModUpdateFn fn_update=nullptr; ModShutdownFn fn_shutdown=nullptr;
    int parsed=0;
};
enum class NativeModStatus { Unknown, Loaded };
struct LoadedMod {
    bool active=false,fromDirectory=false,nativeInPlace=false;
    uint32_t searchDirIndex=0; int fileIdentity=0;
    fs::path modPath,dir,nativeDir; std::string dirUtf8,nativeDirUtf8;
    Metadata metadata; std::optional<Runtime> runtime;
    std::unique_ptr<ModBundle> bundle;
    std::unique_ptr<ModContext> context;
    std::unique_ptr<ConfigVar<bool>> cvarIsEnabled;
    std::unique_ptr<NativeMod> native;
    NativeModStatus nativeStatus=NativeModStatus::Unknown;
    Info manifestInfo;
};
struct LoadedManifest { Metadata metadata; std::optional<Runtime> runtime; };
std::string nextId="dev.twilitrealm.randomizer";
std::unique_ptr<ModBundle> load_bundle(const fs::path&, bool) {
    if(fault=="bundle") throw std::runtime_error("injected bundle failure");
    return std::make_unique<ModBundle>();
}
LoadedManifest load_manifest(const fs::path&, ModBundle&) {
    if(fault=="manifest") throw std::runtime_error("injected manifest failure");
    return {{nextId,"test","1.0.0","test"},{}};
}
int file_identity(const fs::path&) { return 1; }
std::string mod_enabled_cvar_name(std::string_view id) { return std::string(id); }
void fail_mod(LoadedMod& mod, ModResult, const std::string&) { mod.active=false; }
ModContext* nativeContext=nullptr;
Info build_manifest_info(int) {
    if(fault=="native-manifest") throw std::runtime_error("injected native manifest failure");
    return {};
}
struct SearchDir { bool inPlaceNative=true; };
class ModLoader {
public:
    std::vector<std::unique_ptr<LoadedMod>> m_mods;
    std::vector<SearchDir> m_searchDirs{{true}};
    fs::path m_cacheDir;
    LoadedMod* find_mod(const std::string& id) {
        for(auto& mod:m_mods) if(mod->metadata.id==id) return mod.get();
        return nullptr;
    }
    bool load_native_if_present(LoadedMod& mod) {
        if(fault=="native") throw std::runtime_error("injected enumeration failure");
        mod.native=std::make_unique<NativeMod>();
        mod.native->contextSymbol=&nativeContext;
        nativeContext=mod.context.get(); return true;
    }
    LoadedMod* try_load_mod(const fs::path&,bool,uint32_t,std::unique_ptr<ModBundle> = {});
};
} // namespace fixture
const ModMeta mod_meta{};
ModContext* mod_ctx=nullptr;
ModResult mod_initialize(ModError*) {return MOD_OK;}
ModResult mod_update(ModError*) {return MOD_OK;}
ModResult mod_shutdown(ModError*) {return MOD_OK;}
namespace fixture {
bool parse_meta(NativeMod&,LoadedMod&) {return true;}
struct Modal;
struct Action { std::string label; std::function<void(Modal&)> onPressed; };
struct Modal {
    struct Props { std::string title,bodyText; std::vector<Action> actions; std::function<void(Modal&)> onDismiss; std::string icon; };
    Props props; bool popped=false;
    explicit Modal(Props p):props(std::move(p)) {}
    void pop() {popped=true;}
};
std::unique_ptr<Modal> shown;
void push(std::unique_ptr<Modal> modal) {shown=std::move(modal);}
int hookActivations=0;
void play() { GATE ++hookActivations; }
std::string read_file(const fs::path& p) { std::ifstream input(p);return {std::istreambuf_iterator<char>(input),{}}; }
FUNCTION
BUILTIN
} // namespace fixture
using namespace fixture;
int main(int argc,char** argv) {
    assert(argc==2); ConfigPath=fs::path(argv[1]); fs::create_directories(ConfigPath);
    const auto limit=budget.limit;
    for(const auto& phase:{"bundle","manifest","config","native","native-manifest","log"}) {
        ModLoader loader; loader.m_cacheDir=ConfigPath/"cache"; fault=phase; nativeContext=nullptr;
        assert(loader.try_load_mod(ConfigPath/"package",true,0)==nullptr);
        assert(loader.m_mods.empty()); assert(nativeContext==nullptr);
    }
    fault.clear(); ModLoader loader; loader.m_cacheDir=ConfigPath/"cache";
    assert(loader.try_load_mod(ConfigPath/"package",true,5)==nullptr); assert(loader.m_mods.empty());
    auto* first=loader.try_load_mod(ConfigPath/"package",true,0);
    assert(first && first->cvarIsEnabled->value && first->context->mod==first);
    assert(nativeContext==first->context.get());
    assert(loader.try_load_mod(ConfigPath/"duplicate",true,0)==nullptr);
    assert(loader.m_mods.size()==1 && loader.m_mods[0].get()==first);
    nextId="com.optional.asset";
    auto* optional=loader.try_load_mod(ConfigPath/"optional",true,0);
    assert(optional && !optional->cvarIsEnabled->value);
    nextId="existing.local.asset"; loader.m_searchDirs[0].inPlaceNative=false;
    auto* local=loader.try_load_mod(ConfigPath/"local",true,0);
    assert(local && local->cvarIsEnabled->value && loader.m_mods.size()==3);
    LoadedMod candidate; candidate.metadata={"dev.twilitrealm.randomizer","test","1.0.0","test"};
    candidate.context=std::make_unique<ModContext>(); candidate.modPath=ConfigPath;
    fault="path-conversion"; bool threw=false;
    try { load_builtin_randomizer(candidate,ConfigPath/"cache"); } catch(const std::exception&) {threw=true;}
    assert(threw && mod_ctx==nullptr && !candidate.native);
    fault.clear(); assert(load_builtin_randomizer(candidate,ConfigPath/"cache"));
    assert(candidate.native && mod_ctx==candidate.context.get()); mod_ctx=nullptr;
    budget.limit=1ull<<30; play(); assert(shown && hookActivations==0);
    assert(shown->props.bodyText.find("Game")!=std::string::npos);
    shown->props.actions.front().onPressed(*shown); assert(shown->popped); shown.reset();
    budget.limit=limit; play(); assert(!shown && hookActivations==1);
    budget.available=false; budget.limit=1ull<<30; play(); assert(hookActivations==2);
    budget.available=true; budget.xbox=false; play(); assert(hookActivations==3);
    budget.xbox=true; budget.limit=0; play(); assert(hookActivations==4);
    budget.limit=limit; budget.usage=12345;
    xbox_memory::record(ConfigPath,"native.metadata-parse","dev.example.mod","1.2.3","native error\nline");
    auto report=read_file(ConfigPath/"xbox-mod-load.txt");
    assert(report.find("dev.example.mod")!=std::string::npos && report.find("12345/")!=std::string::npos);
    assert(report.find("native error line")!=std::string::npos);
    xbox_memory::record(ConfigPath,"mods.package-ready","next");
    assert(read_file(ConfigPath/"xbox-mod-load-failure.txt").find("native error line")!=std::string::npos);
    auto badPath=ConfigPath/"not-a-folder"; std::ofstream(badPath)<<"file";
    xbox_memory::record(badPath,"operation","id");
    startup_guard::throwStage=true; xbox_memory::record(ConfigPath,"operation","id");
    std::cout<<"PASS .739 production package faults leave no partial entries or stale native contexts; defaults, budget-gated Play and durable diagnostics preserved\n";
}
'''
driver = driver.replace('FUNCTION', function).replace('BUILTIN', builtin).replace('GATE', gate)
with tempfile.TemporaryDirectory(prefix='tpr-739-test-') as directory:
    work = Path(directory)
    (work/'dusk').mkdir()
    (work/'dusk/startup_guard.hpp').write_text('''#pragma once
#include <string>
#include <string_view>
#include <stdexcept>
namespace dusk::startup_guard {
inline bool throwStage=false;
inline std::string last;
inline void stage(std::string_view value) { if(throwStage) throw std::runtime_error("diagnostic fault"); last=value; }
}
''')
    cpp = work/'production-package.cpp'
    cpp.write_text(driver, encoding='utf-8')
    compiler = shutil.which('cl' if os.name=='nt' else 'g++')
    if not compiler:
        raise RuntimeError('C++ compiler is required')
    exe = work/('package-test.exe' if os.name=='nt' else 'package-test')
    if os.name=='nt':
        cmd=[compiler,'/nologo','/std:c++20','/EHsc','/D_UWP=1',str(cpp),'/Fe:'+str(exe)]
        cmd += ['/I'+str(path) for path in [work,root/'src',root/'sdk/include']]
    else:
        cmd=[compiler,'-std=c++20','-D_UWP=1',str(cpp),'-o',str(exe)]
        cmd += ['-I'+str(path) for path in [work,root/'src',root/'sdk/include']]
    subprocess.run(cmd,cwd=work,check=True)
    subprocess.run([str(exe),str(work/'state')],cwd=work,check=True)
    if os.name=='nt':
        wrapper = (root/'platforms/uwp/main.cpp').read_text()
        query = wrapper[wrapper.index('extern "C" bool dusk_query_memory_budget('):wrapper.index('int bootstrap(')]
        includes = wrapper[:wrapper.index('#define SDL_MAIN_HANDLED')]
        cpp.write_text(includes+'\n#include <cassert>\n'+query+'''
int main() {
    std::uint64_t usage=0,limit=0;bool xbox=false;
    assert(!dusk_query_memory_budget(nullptr,&limit,&xbox));
    assert(!dusk_query_memory_budget(&usage,nullptr,&xbox));
    assert(!dusk_query_memory_budget(&usage,&limit,nullptr));
    dusk_query_memory_budget(&usage,&limit,&xbox);
}
''')
        # Match the package's DLL runtime linkage. Desktop /MT brings its own
        # system DLL loader into the test image and obscures probe imports.
        probe_cmd=[compiler,'/nologo','/std:c++20','/EHsc','/MD',str(cpp),'/Fe:'+str(exe),'/link','WindowsApp.lib']
        subprocess.run(probe_cmd,cwd=work,check=True)
        subprocess.run([str(exe)],cwd=work,check=True)
        imports = subprocess.run(['dumpbin','/nologo','/imports',str(exe)],check=True,
            capture_output=True,text=True).stdout
        if 'loadlibraryexw' in imports.lower():
            print(imports)
        for forbidden in ['LoadLibraryExW','GetModuleHandleExA','GetModuleHandleExW']:
            assert forbidden.lower() not in imports.lower(), 'Budget probe imports '+forbidden
        assert 'RoGetActivationFactory'.lower() in imports.lower(), 'Budget probe lost the direct SDK call'
        print('PASS .739 production WinRT budget probe compiles and contains unavailable-runtime/null-pointer failures')
        print('PASS .739 direct SDK budget probe has no desktop module-loader imports')
        # The prior projection call must fail the same import gate with the
        # same CRT linkage, proving that /MD did not hide the original issue.
        cpp.write_text('''#include <winrt/Windows.System.h>
int main() {
    try { return winrt::Windows::System::MemoryManager::AppMemoryUsage() == 0 ? 0 : 1; }
    catch (...) { return 0; }
}
''')
        subprocess.run(probe_cmd,cwd=work,check=True)
        rejected = subprocess.run(['dumpbin','/nologo','/imports',str(exe)],check=True,
            capture_output=True,text=True).stdout
        assert 'loadlibraryexw' in rejected.lower(), 'Former projection no longer reproduces the rejected import'
        print('PASS .739 negative control rejects the former projection under identical DLL runtime linkage')
