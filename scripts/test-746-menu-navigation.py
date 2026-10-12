"""Exercise shipped ModsWindow/Pane/controls with the pinned real RmlUi engine."""
import argparse, os, re, shutil, subprocess, tempfile
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument('source',type=Path)
p.add_argument('--feed',type=Path)
p.add_argument('--rml-source',type=Path)
p.add_argument('--rml-library',type=Path)
p.add_argument('--freetype-library',type=Path)
p.add_argument('--fmt-source',type=Path)
p.add_argument('--negative-control',action='store_true')
a=p.parse_args();root=a.source.resolve();control=Path(__file__).resolve().parents[1]
rml=(a.rml_source or a.feed/'_deps/rmlui-src').resolve()
fmt=(a.fmt_source or a.feed/'_deps/fmt-src').resolve()
libs=[a.rml_library or next(a.feed.rglob('rmlui.lib'))]
if a.freetype_library:libs.append(a.freetype_library)
else:
 found={}
 for folder in [a.feed,Path(os.environ.get('TPR_UWP_DEP',a.feed))]:
  for lib in folder.rglob('*.lib'):
   if re.fullmatch(r'(freetype|freetype2|z|zlib|zlibstatic|zlib-ng|zlib1)\.lib',lib.name,re.I):found.setdefault(lib.name.lower(),lib)
 assert any('freetype' in name for name in found),'Freetype library missing'
 libs.extend(found.values())
libs=[Path(lib).resolve() for lib in libs]

with tempfile.TemporaryDirectory(prefix='menu-navigation-746-') as temp:
 work=Path(temp);ui=work/'ui';include=work/'include';ui.mkdir()
 def write(name,text):
  path=work/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text(text,encoding='utf-8',newline='\n')
 files=['mods_window.cpp','mods_window.hpp','mod_panel_delay.hpp','pane.cpp','pane.hpp',
        'window.cpp','window.hpp','document.cpp','document.hpp','component.cpp','component.hpp',
        'button.cpp','button.hpp','icon_button.cpp','icon_button.hpp','tab_bar.cpp','tab_bar.hpp',
        'event.cpp','event.hpp','nav_types.hpp','format.hpp','mod_texture_provider.hpp']
 for name in files:shutil.copyfile(root/'src/dusk/ui'/name,ui/name)
 if a.negative_control:
  baseline=work/'baseline'
  for name in ('mods_window.cpp','mods_window.hpp','mod_panel_delay.hpp','pane.cpp','pane.hpp'):
   target=baseline/'src/dusk/ui'/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes((ui/name).read_bytes().replace(b'\r\n',b'\n'))
  patch=work/'rollback.patch';patch.write_bytes((control/'patches/xbox-menu-navigation-746.patch').read_bytes().replace(b'\r\n',b'\n'))
  subprocess.run(['git','-C',str(baseline),'apply','--reverse',str(patch)],check=True)
  for name in ('mods_window.cpp','mods_window.hpp','mod_panel_delay.hpp','pane.cpp','pane.hpp'):shutil.copyfile(baseline/'src/dusk/ui'/name,ui/name)
 # Only allow fixture inspection of the shipped window DOM; behavior is untouched.
 for name in ('window.hpp','document.hpp'):
  path=ui/name;path.write_text(path.read_text().replace('protected:','public:'),encoding='utf-8')
 write('ui/ui.hpp',r'''
#pragma once
#include "nav_types.hpp"
#include <RmlUi/Core.h>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <string_view>
namespace dusk::ui {
class Document;
enum class DocumentScope { None,Window };
enum {kSoundItemFocus,kSoundWindowOpen,kSoundWindowClose,kSoundMenuOpen,kSoundMenuClose,kSoundTabChanged};
struct Insets {float top=0,right=0,bottom=0,left=0;bool operator==(const Insets&)const=default;};
extern Rml::Context* testContext;
Document* top_document() noexcept;
Document& push_document(std::unique_ptr<Document>,bool=true,bool=false) noexcept;
void uncover_top_document() noexcept;
void apply_scoped_styles(Document&) noexcept;
bool game_obscured_below(const Document&) noexcept;
Rml::Element* append(Rml::Element*,const Rml::String&) noexcept;
Rml::Element* append_text(Rml::Element*,const Rml::String&) noexcept;
Rml::Element* append_text_element(Rml::Element*,const Rml::String&,const Rml::String&) noexcept;
void clear_children(Rml::Element*) noexcept;
void set_text_content(Rml::Element*,const Rml::String&) noexcept;
std::string escape(std::string_view) noexcept;
NavCommand map_nav_event(const Rml::Event&) noexcept;
Insets safe_area_insets(Rml::Context*) noexcept;
template<class T,class F> void pop_to_or_push(F&&) {}
}
''')
 original=(root/'src/dusk/ui/ui.cpp').read_text(encoding='utf-8')
 helpers=original[original.index('std::string escape('):original.index('void set_display(')]
 nav=original[original.index('NavCommand map_nav_event('):original.index('Insets safe_area_insets(')]
 write('ui/helpers.cpp','#include "ui.hpp"\n#include <string>\nnamespace dusk::ui {\n'+helpers+nav+'\nInsets safe_area_insets(Rml::Context*) noexcept {return {};}\n}\n')
 write('ui/tooltip.hpp','#pragma once\n#include <RmlUi/Core.h>\nnamespace dusk::ui {class Tooltip {public:Tooltip(Rml::Element*,const Rml::String&){} void update(){} void set_label(const Rml::String&){} };}\n')
 for header,klass in [('group_button.hpp','GroupButton'),('select_button.hpp','ControlledSelectButton')]:
  write('ui/'+header,'#pragma once\n#include "button.hpp"\nnamespace dusk::ui {class '+klass+' :public Button {public:using Props=Button::Props;using Button::Button;};}\n')
 write('ui/context_menu.hpp',r'''
#pragma once
#include "document.hpp"
namespace dusk::ui {class ContextMenu {public:
struct Item {Rml::String text,icon;std::function<void()> onPressed;bool enabled=true,destructive=false,separatorBefore=false,selected=false;};
class Binding {public:Binding(Document&,Rml::Element*,Rml::String,std::function<std::vector<Item>(Rml::Element*)>){} void dismiss(){} };
};}
''')
 write('ui/logs_window.hpp','#pragma once\n#include "document.hpp"\nnamespace dusk::ui {class LogsWindow:public Document {public:explicit LogsWindow(std::string):Document("<rml><body/></rml>"){} };}\n')
 write('ui/modal.hpp',r'''
#pragma once
#include "document.hpp"
namespace dusk::ui {class Modal;struct ModalAction {Rml::String text;std::function<void(Modal&)> callback;Rml::String icon;};
class Modal:public Document {public:struct Props {Rml::String title,bodyText;std::vector<ModalAction> actions;Rml::String variant,icon;};
explicit Modal(Props):Document("<rml><body/></rml>"){} };}
''')
 write('ui/online_mods.hpp','#pragma once\n#include "pane.hpp"\nnamespace dusk::ui {class ModsWindow;inline void build_online_mods(Pane& p,ModsWindow&,std::unordered_set<std::string>&){p.add_text("Online mods");}}\n')
 write('ui/mod_updates.hpp','#pragma once\n#include "component.hpp"\nnamespace dusk::ui {inline void set_mod_update_badge(Component&,std::string_view){} inline void enqueue_mod_update(std::string_view){} }\n')
 write('ui/package_row.hpp','#pragma once\n')
 write('include/dusk/mod_loader.hpp',r'''
#pragma once
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
using u32=uint32_t;
namespace dusk::mods {
struct ModManifestInfo {struct Import {uint32_t id=0;};std::vector<Import> imports;};
struct LoadedMod {struct Metadata {std::string id,name,version,author,description,iconPath,bannerPath; } metadata;
bool active=false,loadFailed=false,suspendedByProvider=false,nativeInPlace=true,hasBundledCopy=true,fromDirectory=false,enabled=false;
u32 cacheGeneration=1;std::string failureReason;std::filesystem::path modPath;
ModManifestInfo manifestInfo;struct Edge {bool required=false;LoadedMod* mod=nullptr;};std::vector<Edge> dependencies,dependents;
bool is_enabled() const{return enabled;} bool activation_failed() const{return loadFailed;}};
class ModLoader {public:std::vector<LoadedMod> records;uint64_t revision=1;std::vector<std::string> actions;
static ModLoader& instance(){static ModLoader loader;return loader;} auto& mods(){return records;} uint64_t generation(){return revision;}
LoadedMod* find_mod(std::string_view id){for(auto& m:records)if(m.metadata.id==id)return &m;return nullptr;}
bool can_uninstall(const LoadedMod&){return false;}
void request_reactivate(std::string_view id){actions.emplace_back(id);} void request_reload(std::string_view id){actions.emplace_back(id);}
void request_enable(std::string_view id){actions.emplace_back(id);} void request_disable(std::string_view id){actions.emplace_back(id);}
void request_uninstall(std::string_view id){actions.emplace_back(id);}
};}
''')
 write('include/dusk/mods/updates.hpp','#pragma once\n#include <string_view>\nnamespace dusk::mods::updates {struct Entry{bool actionable=false;};inline uint64_t generation(){return 1;} inline const Entry* find(std::string_view){return nullptr;} }\n')
 write('include/dusk/mods/queue.hpp','#pragma once\n#include <string>\n#include <vector>\nnamespace dusk::mods::queue {struct Item{std::string id;int state=0;};inline auto items(){return std::vector<Item>{};} inline bool is_completed(int){return false;} }\n')
 write('include/dusk/mods/svc/net.hpp','#pragma once\nnamespace dusk::mods::svc {inline bool is_network_service(uint32_t){return false;}}\n')
 write('include/dusk/mods/svc/ui.hpp',r'''
#pragma once
#include "pane.hpp"
#include "dusk/mod_loader.hpp"
namespace dusk::mods::svc {inline std::vector<std::string> panels,updates;
inline void ui_build_mods_panels(LoadedMod& m,dusk::ui::Pane& p){panels.push_back(m.metadata.id);p.add_button("Native option");}
inline void ui_update_mods_panels(LoadedMod& m){updates.push_back(m.metadata.id);}}
''')
 write('include/dusk/startup_guard.hpp','#pragma once\n#include <string>\n#include <vector>\nnamespace dusk::startup_guard {inline std::vector<std::string> stages;inline void stage(std::string_view s){stages.emplace_back(s);}}\n')
 write('include/dusk/data.hpp','#pragma once\n#include <filesystem>\nnamespace dusk::data {struct Manager{struct Capabilities{bool canOpenFolder=false;};auto capabilities(){return Capabilities{};} bool open_folder(const std::filesystem::path&){return false;} };inline Manager& manager(){static Manager m;return m;}}\n')
 write('include/aurora/rmlui.hpp','#pragma once\n#include "ui.hpp"\nnamespace aurora::rmlui {inline Rml::Context* get_context(){return dusk::ui::testContext;}}\n')
 for name in ('borealis/http.hpp','mods/svc/http.h','Z2AudioLib/Z2SeMgr.h','aurora/lib/window.hpp','magic_enum.hpp'):write('include/'+name,'#pragma once\n')
 write('include/m_Do/m_Do_audio.h','#pragma once\ninline void mDoAud_seStartMenu(int){}\n')
 write('include/tracy/Tracy.hpp','#pragma once\n#define ZoneScopedN(...)\n')
 shutil.copytree(root/'res/rml',work/'res/rml')
 shutil.copyfile(root/'res/FiraSans-Regular.ttf',work/'font.ttf')
 # Use the author's actual image decorator so lazy icon loading is exercised.
 shutil.copyfile(root/'extern/aurora/lib/rmlui/ImageEffects.cpp',work/'ImageEffects.cpp')
 shutil.copyfile(root/'extern/aurora/lib/rmlui/ImageEffects.hpp',work/'ImageEffects.hpp')
 shutil.copyfile(control/'tests/mod_menu_navigation_746.cpp',work/'main.cpp')
 sources=[str(path) for path in ui.glob('*.cpp')]+[str(work/'ImageEffects.cpp')]
 exe=work/('menu.exe' if os.name=='nt' else 'menu')
 if os.name=='nt':
  cmd=['cl','/nologo','/std:c++20','/EHsc','/MD','/utf-8','/DRMLUI_STATIC_LIB','/DFMT_HEADER_ONLY','/I'+str(include),'/I'+str(ui),'/I'+str(rml/'Include'),'/I'+str(fmt/'include'),str(work/'main.cpp'),*sources,'/Fe:'+str(exe),'/link',*map(str,libs),'user32.lib']
  for folder in [a.feed,Path(os.environ.get('TPR_UWP_DEP',a.feed))]:
   for dll in folder.rglob('*.dll'):
    if re.fullmatch(r'(freetype|freetype2|z|zlib|zlib1)\.dll',dll.name,re.I):shutil.copyfile(dll,work/dll.name)
 else:cmd=['c++','-std=c++20','-O1','-pthread','-DRMLUI_STATIC_LIB','-DFMT_HEADER_ONLY','-I'+str(include),'-I'+str(ui),'-I'+str(rml/'Include'),'-I'+str(fmt/'include'),str(work/'main.cpp'),*sources,*map(str,libs),'-o',str(exe)]
 subprocess.run(cmd,cwd=work,check=True)
 result=subprocess.run([str(exe)]+(['negative'] if a.negative_control else []),cwd=work)
 if a.negative_control:assert result.returncode==42,'Shipped .745 navigation churn was not reproduced'
 elif result.returncode:raise SystemExit(result.returncode)
