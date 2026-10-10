"""Exercise production picker/components against the pinned real RmlUi layout engine."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('source', type=Path)
p.add_argument('--feed', type=Path)
p.add_argument('--rml-source', type=Path)
p.add_argument('--rml-library', type=Path)
p.add_argument('--freetype-library', type=Path)
args = p.parse_args()
root = args.source.resolve()
rml = args.rml_source or args.feed/'_deps/rmlui-src'
assert rml.is_dir(), 'Pinned RmlUi source is missing'
rml_lib = args.rml_library
if rml_lib is None:
    rml_lib = next(args.feed.rglob('rmlui.lib'))
libs = [rml_lib]
if args.freetype_library:
    libs.append(args.freetype_library)
else:
    search_roots = [args.feed]
    if os.environ.get('TPR_UWP_DEP'):
        search_roots.append(Path(os.environ['TPR_UWP_DEP']))
    found = {}
    for folder in search_roots:
        for lib in folder.rglob('*.lib'):
            if re.fullmatch(r'(freetype|freetype2|zlib|zlibstatic|zlib-ng|zlib1)\.lib', lib.name, re.I):
                found.setdefault(lib.name.lower(), lib)
    assert any('freetype' in name for name in found), 'Freetype link library missing'
    libs.extend(found.values())

ui = root/'src/dusk/ui'
original_ui = (ui/'ui.cpp').read_text(encoding='utf-8')
original_window = (ui/'window.cpp').read_text(encoding='utf-8')
small_source = re.search(r'const Rml::String kDocumentSourceSmall = R"RML\(.*?\)RML";', original_window, re.S).group()
small_functions = original_window[original_window.index('WindowSmall::WindowSmall'):original_window.rindex('}  // namespace dusk::ui')]
helpers = original_ui[original_ui.index('Rml::Element* append('):original_ui.index('void set_display(')]
navigation = original_ui[original_ui.index('NavCommand map_nav_event('):original_ui.index('Insets safe_area_insets(')]

with tempfile.TemporaryDirectory(prefix='tpr-738-rmlui-') as temp:
    work = Path(temp)
    fixture_ui = work/'ui'
    include = work/'include'
    fixture_ui.mkdir()
    def write(name, text):
        path = work/name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8', newline='\n')
    # No UI component is reimplemented. Only external game/audio/SDL/tooltip services
    # are stubbed; WindowSmall and helper functions are extracted byte-for-byte.
    files = ['xbox_file_picker.cpp', 'xbox_file_picker.hpp', 'xbox_file_picker_model.hpp',
             'modal.cpp', 'modal.hpp', 'pane.cpp', 'pane.hpp', 'list.cpp', 'list.hpp',
             'button.cpp', 'button.hpp', 'component.cpp', 'component.hpp',
             'document.cpp', 'document.hpp', 'event.cpp', 'event.hpp', 'nav_types.hpp']
    for name in files:
        shutil.copyfile(ui/name, fixture_ui/name)
    write('include/borealis/file_select.hpp', (root/'extern/borealis/include/borealis/file_select.hpp').read_text())
    write('ui/ui.hpp', r'''
#pragma once
#include "nav_types.hpp"
#include <RmlUi/Core.h>
#include <memory>
#include <m_Do/m_Do_audio.h>
namespace dusk::ui {
class Document;
enum class DocumentScope { None, Window };
enum { kSoundItemFocus, kSoundWindowOpen, kSoundWindowClose, kSoundMenuOpen, kSoundMenuClose };
extern Rml::Context* testContext;
Document* top_document() noexcept;
void push_document(std::unique_ptr<Document>, bool = true, bool = false) noexcept;
void uncover_top_document() noexcept;
void apply_scoped_styles(Document&) noexcept;
bool game_obscured_below(const Document&) noexcept;
Rml::Element* append(Rml::Element*, const Rml::String&) noexcept;
Rml::Element* append_text(Rml::Element*, const Rml::String&) noexcept;
Rml::Element* append_text_element(Rml::Element*, const Rml::String&, const Rml::String&) noexcept;
void clear_children(Rml::Element*) noexcept;
void set_text_content(Rml::Element*, const Rml::String&) noexcept;
NavCommand map_nav_event(const Rml::Event&) noexcept;
}
''')
    write('ui/window.hpp', r'''
#pragma once
#include "document.hpp"
namespace dusk::ui {
class WindowSmall : public Document {
public:
    explicit WindowSmall(const Rml::String&);
    void show() override;
    void hide(bool) override;
    bool visible() const override;
protected:
    Rml::Element* mRoot = nullptr;
    Rml::Element* mDialog = nullptr;
};
}
''')
    write('ui/tooltip.hpp', r'''
#pragma once
#include <RmlUi/Core.h>
namespace dusk::ui {
class Tooltip { public:
    Tooltip(Rml::Element*, const Rml::String&) {}
    void update() {}
    void set_label(const Rml::String&) {}
};
}
''')
    for header, klass in [('group_button.hpp', 'GroupButton'), ('select_button.hpp', 'ControlledSelectButton')]:
        write('ui/'+header, '#pragma once\n#include "button.hpp"\nnamespace dusk::ui { class '+klass+
              ' : public Button { public: using Props=Button::Props; using Button::Button; }; }\n')
    write('include/aurora/rmlui.hpp', '#pragma once\n#include "ui.hpp"\nnamespace aurora::rmlui { inline Rml::Context* get_context() { return dusk::ui::testContext; } }\n')
    write('include/Z2AudioLib/Z2SeMgr.h', '#pragma once\n')
    write('include/m_Do/m_Do_audio.h', '#pragma once\ninline void mDoAud_seStartMenu(int) {}\n')
    write('include/borealis/io.hpp', r'''
#pragma once
#include <filesystem>
#include <string>
namespace borealis::io {
inline std::string fs_path_to_string(const std::filesystem::path& p) {
    auto s=p.u8string(); return {reinterpret_cast<const char*>(s.data()),s.size()};
}
}
''')
    write('include/dusk/data.hpp', r'''
#pragma once
#include <filesystem>
namespace dusk::data {
extern std::filesystem::path configured;
inline std::filesystem::path configured_data_path() { return configured; }
inline std::filesystem::path cache_path() { return configured.parent_path()/"LocalCache"; }
}
''')
    write('include/SDL3/SDL_filesystem.h', '#pragma once\n#include <string>\nextern std::string testBasePath; inline const char* SDL_GetBasePath() { return testBasePath.c_str(); }\n')
    write('include/SDL3/SDL_log.h', '#pragma once\n')
    write('ui/window_small.cpp', '#include "window.hpp"\nnamespace dusk::ui {\n'+small_source+'\n'+small_functions+'\n}\n')
    write('ui/helpers.cpp', '#include "ui.hpp"\nnamespace dusk::ui {\n'+helpers+'\n'+navigation+'\n}\n')
    for name in ('theme.rcss', 'window.rcss', 'controls.rcss', 'global.rcss'):
        write('res/rml/'+name, (root/'res/rml'/name).read_text())
    fonts = sorted((root/'res').glob('FiraSans*Regular.ttf'))
    assert fonts, 'Production Fira Sans fonts missing'
    font = next(path for path in fonts if path.name == 'FiraSans-Regular.ttf')
    shutil.copyfile(font, work/'font.ttf')
    cpp = Path(__file__).resolve().parent.parent/'tests/xbox-picker-navigation-738.cpp'
    shutil.copyfile(cpp, work/'main.cpp')
    sources = [str(path) for path in fixture_ui.glob('*.cpp') if path.name != 'xbox_file_picker.cpp']
    exe = work/('picker_ui.exe' if os.name == 'nt' else 'picker_ui')
    if os.name == 'nt':
        command = ['cl', '/nologo', '/std:c++20', '/EHsc', '/MD', '/utf-8', '/D_UWP=1', '/DRMLUI_STATIC_LIB',
                   '/I'+str(include), '/I'+str(fixture_ui), '/I'+str(rml/'Include'),
                   str(work/'main.cpp'), *sources, '/Fe:'+str(exe), '/link', *map(str, libs)]
        for folder in [args.feed, Path(os.environ.get('TPR_UWP_DEP', args.feed))]:
            for dll in folder.rglob('*.dll'):
                if re.match(r'(freetype|zlib|zlib1)', dll.name, re.I):
                    shutil.copyfile(dll, work/dll.name)
    else:
        command = ['g++', '-std=c++20', '-pthread', '-D_UWP=1', '-DRMLUI_STATIC_LIB',
                   '-I'+str(include), '-I'+str(fixture_ui), '-I'+str(rml/'Include'),
                   str(work/'main.cpp'), *sources, *map(str, libs), '-o', str(exe)]
    subprocess.run(command, cwd=work, check=True)
    subprocess.run([str(exe)], cwd=work, check=True)
