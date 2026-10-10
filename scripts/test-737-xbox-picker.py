"""Compile production UWP picker dispatch and filesystem browsing with injected failures."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('source', nargs='?', default=os.environ.get('TPR_SRC'))
root = Path(parser.parse_args().source)
library = root/'extern/borealis'
source = (library/'src/file_select/file_select.cpp').read_bytes()
assert 'target_compile_definitions(borealis_file_select PRIVATE _UWP=1)' in (library/'CMakeLists.txt').read_text()
assert 'target_sources(dusk_internal PRIVATE src/dusk/ui/xbox_file_picker.cpp)' in (root/'CMakeLists.txt').read_text()
ui = (root/'src/dusk/ui/ui.cpp').read_text()
assert ui.index('xbox_file_picker::update();') < ui.index('update_documents(sDocumentStack')
assert 'xbox_file_picker::shutdown();' in ui
settings = (root/'src/dusk/ui/settings.cpp').read_text()
assert 'set_custom_data_path(borealis::io::fs_path_from_utf8(result.locations.front())' in settings
assert 'Data Folder Not Changed' in settings and 'catch (const std::exception& exception)' in settings

sdl = r'''
#pragma once
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
struct SDL_Window {};
struct SDL_DialogFileFilter { const char* name; const char* pattern; };
using DialogCallback = void (*)(void*, const char* const*, int);
inline const std::thread::id mainThread = std::this_thread::get_id();
inline std::atomic_bool failQueue = false;
inline int desktopCalls = 0, callbackErrors = 0;
inline std::mutex queueMutex;
inline std::vector<std::pair<void (*)(void*), void*>> queue;
inline bool SDL_IsMainThread() { return std::this_thread::get_id() == mainThread; }
inline bool SDL_RunOnMainThread(void (*callback)(void*), void* state, bool) {
    if (failQueue.load()) return false;
    std::lock_guard lock(queueMutex); queue.emplace_back(callback, state); return true;
}
inline const char* SDL_GetError() { return "Injected dialog failure"; }
constexpr int SDL_LOG_CATEGORY_APPLICATION = 0;
inline void SDL_LogError(int, const char*, ...) { ++callbackErrors; }
inline void SDL_ShowOpenFileDialog(DialogCallback, void*, SDL_Window*, const SDL_DialogFileFilter*, int, const char*, bool) { ++desktopCalls; }
inline void SDL_ShowOpenFolderDialog(DialogCallback, void*, SDL_Window*, const char*, bool) { ++desktopCalls; }
inline void SDL_ShowSaveFileDialog(DialogCallback, void*, SDL_Window*, const SDL_DialogFileFilter*, int, const char*) { ++desktopCalls; }
inline void drain() {
    std::vector<std::pair<void (*)(void*), void*>> pending;
    { std::lock_guard lock(queueMutex); pending.swap(queue); }
    for (auto [callback, state] : pending) callback(state);
}
'''
io = r'''
#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
namespace borealis::io {
enum class Status { Ok, AlreadyExists, Failed };
struct File {
    enum class Mode { Read, Truncate };
    uint64_t read(void*, size_t) { return 0; }
    bool write(std::span<std::byte>) { return true; }
    bool close() { return true; }
    std::string error() { return {}; }
};
struct OpenResult { Status status = Status::Ok; File file; std::string message; };
struct JoinResult { Status status = Status::Ok; std::string location, message; };
inline OpenResult open(std::string_view, File::Mode) { return {}; }
inline Status check(std::string_view source) { return source == "missing" ? Status::Failed : Status::Ok; }
inline std::filesystem::path fs_path_from_utf8(std::string_view value) { return std::filesystem::u8path(value.begin(), value.end()); }
inline std::string fs_path_to_string(const std::filesystem::path& path) {
    auto value = path.u8string(); return {reinterpret_cast<const char*>(value.data()), value.size()};
}
inline JoinResult create_child(std::string_view, std::string_view) { return {}; }
inline bool atomic_replace(const std::filesystem::path&, const std::filesystem::path&, std::string&) { return true; }
namespace detail {
inline bool safe_child_name(std::string_view name) { return !name.empty() && name.find_first_of("/\\:") == name.npos && name != ".."; }
}
}
'''
main = r'''
#include <cassert>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "file_select/production.cpp"
#include "xbox_file_picker_model.hpp"
namespace s = borealis::file_select;
namespace model = dusk::ui::xbox_file_picker::model;
s::Callback pending;
int mode = 0;
template <class Options> void backend(Options, s::Callback callback) {
    if (mode == 1) throw std::runtime_error("Backend startup failed");
    if (mode == 2) { callback({.status=s::Status::Selected, .locations={"chosen"}}); throw std::runtime_error("Late backend exception"); }
    pending = std::move(callback);
}
int main() {
    auto receive = [](s::Result result) { assert(SDL_IsMainThread()); };
    assert(!s::capabilities().canOpenFile && !s::capabilities().canOpenFolder);
    int count = 0;
    s::open_file({}, [&](s::Result r) { ++count; assert(r.status==s::Status::Unsupported); });
    drain(); assert(count==1 && !s::busy());
    s::install_platform_backend({&backend<s::FileOptions>, &backend<s::FolderOptions>, &backend<s::ExportOptions>});
    assert(s::capabilities().canOpenFile && s::capabilities().canOpenFolder && s::capabilities().canExportFile);
    count = 0;
    s::open_file({}, [&](s::Result r) { assert(SDL_IsMainThread()); ++count; assert(r.status==s::Status::Selected); });
    assert(s::busy());
    int busyCount = 0;
    s::open_folder({}, [&](s::Result r) { ++busyCount; assert(r.status==s::Status::Busy); });
    drain(); assert(busyCount==1 && count==0 && s::busy());
    pending({.status=s::Status::Selected, .locations={"disc.RVZ"}});
    pending({.status=s::Status::Canceled});
    drain(); assert(count==1 && !s::busy());
    mode = 1;
    s::open_folder({}, [&](s::Result r) { ++count; assert(r.status==s::Status::Failed && r.message=="Backend startup failed"); });
    drain(); assert(count==2 && !s::busy());
    mode = 2;
    s::open_file({}, [&](s::Result r) { ++count; assert(r.status==s::Status::Selected); });
    drain(); assert(count==3 && !s::busy());
    mode = 0;
    std::string configured = "original data folder";
    count = 0;
    s::open_folder({}, [&](s::Result r) { assert(SDL_IsMainThread()); ++count; if(r.status==s::Status::Selected) configured=r.locations.front(); });
    failQueue = true;
    std::thread worker([&] { pending({.status=s::Status::Canceled}); s::pump_completions(); });
    worker.join(); assert(count==0 && configured=="original data folder");
    s::pump_completions(); assert(count==1 && configured=="original data folder" && !s::busy());
    std::thread wrongThread([&] { s::open_folder({}, [&](s::Result r) { assert(SDL_IsMainThread()); ++count; assert(r.status==s::Status::Failed); }); });
    wrongThread.join(); assert(count==1); s::pump_completions(); assert(count==2);
    failQueue = false;
    s::open_folder({}, [](s::Result) { throw std::runtime_error("UI failure"); });
    pending({.status=s::Status::Failed}); drain(); assert(callbackErrors==1 && !s::busy());
    s::export_file({.sourceLocation="source", .suggestedName="save.gci"}, receive);
    assert(s::busy()); pending({.status=s::Status::Canceled}); drain(); assert(!s::busy());
    s::export_file({.sourceLocation="source", .suggestedName="../save.gci"}, [&](s::Result r) { assert(r.status==s::Status::Failed); }); drain();
    s::install_platform_backend({}); assert(!s::capabilities().canOpenFolder);
    assert(desktopCalls==0);

    const auto base = std::filesystem::temp_directory_path()/"tpr-737-picker-fixture";
    std::filesystem::remove_all(base); std::filesystem::create_directories(base/"folder");
    auto touch = [](const std::filesystem::path& file) { std::ofstream(file, std::ios::binary) << "fixture"; };
    touch(base/"game.RVZ"); touch(base/"game.iso"); touch(base/"ignore.txt"); touch(base/model::path("\xc3\xa9-image.gcm"));
    for (int i=0;i<405;++i) touch(base/("seed-"+std::to_string(i)+".rvz"));
    std::atomic_bool canceled = false;
    auto files = model::scan(model::utf8(base), {}, "iso;gcm;rvz", false, canceled);
    assert(files.usable && files.error.empty() && files.entries.size()==409 && files.entries.front().directory);
    assert(std::any_of(files.entries.begin(),files.entries.end(),[](auto& e){ return e.label=="game.RVZ"; }));
    assert(std::any_of(files.entries.begin(),files.entries.end(),[](auto& e){ return e.label=="\xc3\xa9-image.gcm" && model::utf8(model::path(e.location))==e.location; }));
    auto folders = model::scan(model::utf8(base), {}, "*", true, canceled);
    assert(folders.usable && folders.entries.size()==1 && folders.entries[0].directory);
    auto missing = model::scan(model::utf8(base/"missing"), {}, "*", false, canceled);
    assert(!missing.usable && !missing.error.empty());
    auto notDirectory = model::scan(model::utf8(base/"game.iso"), {}, "*", false, canceled);
    assert(!notDirectory.usable && !notDirectory.error.empty());
    auto roots = model::scan({}, {model::utf8(base),model::utf8(base),model::utf8(base/"missing")}, "*", false, canceled);
    assert(roots.usable && roots.entries.front().location==model::utf8(base) && std::count_if(roots.entries.begin(),roots.entries.end(),[&](auto& e){return e.location==model::utf8(base);})==1);
    canceled = true; auto abandoned = model::scan(model::utf8(base), {}, "*", false, canceled); assert(abandoned.entries.empty());
    std::filesystem::remove_all(base);
    std::cout << "PASS .737 production UWP picker: no desktop dialog calls; main-thread completion under queue failure; once-only, busy, cancel, exceptions, export guards; Unicode, filtering, 409-entry paging source and folder errors\n";
}
'''
with tempfile.TemporaryDirectory(prefix='tpr-737-picker-') as directory:
    work = Path(directory)
    (work/'file_select').mkdir()
    (work/'include/SDL3').mkdir(parents=True)
    (work/'include/borealis').mkdir()
    (work/'include/test_sdl.hpp').write_text(sdl)
    for header in ('SDL_dialog.h', 'SDL_error.h', 'SDL_init.h', 'SDL_log.h'):
        (work/'include/SDL3'/header).write_text('#include "test_sdl.hpp"\n')
    (work/'include/borealis/io.hpp').write_text(io)
    (work/'io_internal.hpp').write_text('#include <borealis/io.hpp>\n')
    (work/'file_select/production.cpp').write_bytes(source)
    shutil.copyfile(library/'src/file_select/file_select_internal.hpp', work/'file_select/file_select_internal.hpp')
    shutil.copyfile(root/'src/dusk/ui/xbox_file_picker_model.hpp', work/'include/xbox_file_picker_model.hpp')
    (work/'main.cpp').write_text(main, encoding='utf-8')
    exe = work/('picker.exe' if os.name=='nt' else 'picker')
    if os.name=='nt':
        compiler = shutil.which('cl.exe') or shutil.which('cl')
        command = [compiler,'/nologo','/std:c++20','/EHsc','/utf-8','/D_UWP=1',
                   '/I'+str(work/'include'),'/I'+str(library/'include'),str(work/'main.cpp'),'/Fe:'+str(exe)]
    else:
        compiler = shutil.which('g++') or shutil.which('clang++')
        command = [compiler,'-std=c++20','-pthread','-D_UWP=1','-I'+str(work/'include'),
                   '-I'+str(library/'include'),str(work/'main.cpp'),'-o',str(exe)]
    subprocess.run(command, cwd=work, check=True)
    subprocess.run([str(exe)], cwd=work, check=True)
