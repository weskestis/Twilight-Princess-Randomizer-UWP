"""Exercise production UWP file callbacks and Borealis File/open methods.

MSVC uses real Win32 filesystem calls with fault injection. Linux uses a small
Win32 shim over real files. The package build separately checks the actual
borealis_io target flag and compiles against the pinned SDL3 headers.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('source', nargs='?', default=os.environ.get('TPR_SRC'))
root = Path(parser.parse_args().source)
library = root / 'extern/borealis'
source = (library / 'src/io.cpp').read_text()
cmake = (library / 'CMakeLists.txt').read_text()
assert 'target_compile_definitions(borealis_io PRIVATE _UWP=1)' in cmake
assert 'SDL_IOFromFP' not in source
assert 'detail::open_uwp_write_stream(nativePath, mode == File::Mode::Append)' in source
assert 'userDir / ".downloads"' in (root / 'src/dusk/mods/queue.cpp').read_text()
catalog = (root / 'src/dusk/mods/catalog.cpp').read_text()
native_start = catalog.index('bool supports_native_installs() noexcept {')
native_policy = catalog[native_start:catalog.index('\n}',native_start)+2]
assert '#if defined(_UWP)' in native_policy
assert 'UWP port required' in (root / 'src/dusk/ui/mod_browser.cpp').read_text()

# Keep the class, move/close/write behavior and open selection from production.
methods = source[source.index('File::~File() {'):source.index('Status check(std::string_view location) {')]
if os.name != 'nt':
    # Only the platform-selection guard changes for the Linux API shim.
    methods = methods.replace('#if defined(_UWP) && defined(_WIN32)', '#if defined(_UWP)')

stubs = r'''
#include <borealis/io.hpp>
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#else
using DWORD = uint32_t; using BOOL = int; using HANDLE = FILE*;
struct LARGE_INTEGER { int64_t QuadPart = 0; };
struct OVERLAPPED {};
struct CREATEFILE2_EXTENDED_PARAMETERS {
    DWORD dwSize = 0, dwFileAttributes = 0, dwFileFlags = 0;
};
inline HANDLE INVALID_HANDLE_VALUE = reinterpret_cast<HANDLE>(intptr_t{-1});
constexpr DWORD GENERIC_READ = 1, GENERIC_WRITE = 2, FILE_SHARE_READ = 4, FILE_SHARE_DELETE = 8;
constexpr DWORD OPEN_ALWAYS = 4, CREATE_ALWAYS = 2, FILE_ATTRIBUTE_NORMAL = 128, FILE_FLAG_SEQUENTIAL_SCAN = 0x08000000;
constexpr DWORD FILE_BEGIN = 0, FILE_CURRENT = 1, FILE_END = 2, ERROR_DISK_FULL = 112, ERROR_ACCESS_DENIED = 5;
constexpr BOOL FALSE = 0;
DWORD testLastError = 0;
void SetLastError(DWORD code) { testLastError = code; }
DWORD GetLastError() { return testLastError; }
#endif
using Sint64 = int64_t;
#define SDLCALL
enum SDL_IOStatus { SDL_IO_STATUS_READY, SDL_IO_STATUS_ERROR, SDL_IO_STATUS_EOF };
enum SDL_IOWhence { SDL_IO_SEEK_SET, SDL_IO_SEEK_CUR, SDL_IO_SEEK_END };
struct SDL_IOStreamInterface {
    uint32_t version;
    Sint64 (*size)(void*);
    Sint64 (*seek)(void*, Sint64, SDL_IOWhence);
    size_t (*read)(void*, void*, size_t, SDL_IOStatus*);
    size_t (*write)(void*, const void*, size_t, SDL_IOStatus*);
    bool (*flush)(void*, SDL_IOStatus*);
    bool (*close)(void*);
};
static_assert(sizeof(SDL_IOStreamInterface) == 56);
#define SDL_INIT_INTERFACE(p) (p)->version = static_cast<uint32_t>(sizeof(*(p)))
struct SDL_IOStream {
    SDL_IOStreamInterface interface{};
    void* userdata = nullptr;
    SDL_IOStatus status = SDL_IO_STATUS_READY;
};
std::string sdlError;
int handles = 0, streams = 0, oldBackendCalls = 0;
DWORD shortWrite = 0, openFailure = 0;
int64_t writeBudget = -1;
bool zeroWrite = false, bindFailure = false, seekFailure = false, flushFailure = false, closeFailure = false;
void SDL_ClearError() { sdlError.clear(); }
const char* SDL_GetError() { return sdlError.c_str(); }
bool SDL_SetError(const char* format, ...) {
    char message[1024]; va_list args; va_start(args, format);
    std::vsnprintf(message, sizeof(message), format, args); va_end(args);
    sdlError = message; return false;
}
SDL_IOStream* SDL_OpenIO(const SDL_IOStreamInterface* interface, void* userdata) {
    if (bindFailure) { SDL_SetError("Injected stream bind failure"); return nullptr; }
    assert(interface->version == sizeof(*interface));
    auto* stream = new SDL_IOStream; stream->interface = *interface; stream->userdata = userdata;
    ++streams; return stream;
}
SDL_IOStream* SDL_IOFromFile(const char*, const char*) {
    ++oldBackendCalls;
    SDL_SetError("Error writing to datastream: There is not enough space on the disk.");
    return nullptr;
}
Sint64 SDL_GetIOSize(SDL_IOStream* f) { return f->interface.size(f->userdata); }
Sint64 SDL_SeekIO(SDL_IOStream* f, Sint64 offset, SDL_IOWhence whence) {
    return f->interface.seek(f->userdata, offset, whence);
}
size_t SDL_ReadIO(SDL_IOStream* f, void* out, size_t size) {
    f->status = SDL_IO_STATUS_READY; return f->interface.read(f->userdata, out, size, &f->status);
}
size_t SDL_WriteIO(SDL_IOStream* f, const void* in, size_t size) {
    f->status = SDL_IO_STATUS_READY; return f->interface.write(f->userdata, in, size, &f->status);
}
SDL_IOStatus SDL_GetIOStatus(SDL_IOStream* f) { return f->status; }
bool SDL_FlushIO(SDL_IOStream* f) {
    f->status = SDL_IO_STATUS_READY; return f->interface.flush(f->userdata, &f->status);
}
bool SDL_CloseIO(SDL_IOStream* f) {
    bool ok = f->interface.close(f->userdata); delete f; --streams; return ok;
}
HANDLE testCreateFile2(const std::filesystem::path::value_type* path, DWORD access, DWORD share,
    DWORD disposition, const CREATEFILE2_EXTENDED_PARAMETERS* parameters) {
    if (openFailure) { SetLastError(openFailure); return INVALID_HANDLE_VALUE; }
#ifdef _WIN32
    HANDLE handle = ::CreateFile2(path, access, share, disposition, parameters);
#else
    (void)access; (void)share; (void)parameters;
    FILE* handle = std::fopen(path, disposition == CREATE_ALWAYS ? "w+b" : "r+b");
    if (!handle && disposition == OPEN_ALWAYS) handle = std::fopen(path, "w+b");
    if (!handle) { SetLastError(ERROR_ACCESS_DENIED); return INVALID_HANDLE_VALUE; }
#endif
    if (handle != INVALID_HANDLE_VALUE) ++handles;
    return handle;
}
BOOL testGetFileSizeEx(HANDLE handle, LARGE_INTEGER* out) {
#ifdef _WIN32
    return ::GetFileSizeEx(handle, out);
#else
    const auto position = ftello(handle);
    if (position < 0 || fseeko(handle, 0, SEEK_END) != 0) return FALSE;
    out->QuadPart = ftello(handle); return fseeko(handle, position, SEEK_SET) == 0;
#endif
}
BOOL testSetFilePointerEx(HANDLE handle, LARGE_INTEGER offset, LARGE_INTEGER* out, DWORD origin) {
    if (seekFailure) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
#ifdef _WIN32
    return ::SetFilePointerEx(handle, offset, out, origin);
#else
    const int whence = origin == FILE_BEGIN ? SEEK_SET : origin == FILE_CURRENT ? SEEK_CUR : SEEK_END;
    if (fseeko(handle, offset.QuadPart, whence) != 0) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
    out->QuadPart = ftello(handle); return true;
#endif
}
BOOL testReadFile(HANDLE handle, void* out, DWORD count, DWORD* read, OVERLAPPED* overlapped) {
#ifdef _WIN32
    return ::ReadFile(handle, out, count, read, overlapped);
#else
    (void)overlapped; *read = static_cast<DWORD>(std::fread(out, 1, count, handle));
    return std::ferror(handle) == 0;
#endif
}
BOOL testWriteFile(HANDLE handle, const void* in, DWORD count, DWORD* written, OVERLAPPED* overlapped) {
    *written = 0;
    if (zeroWrite) return true;
    if (writeBudget == 0) { SetLastError(ERROR_DISK_FULL); return FALSE; }
    if (shortWrite) count = std::min(count, shortWrite);
    if (writeBudget > 0) count = static_cast<DWORD>(std::min<int64_t>(count, writeBudget));
#ifdef _WIN32
    const BOOL ok = ::WriteFile(handle, in, count, written, overlapped);
#else
    (void)overlapped; *written = static_cast<DWORD>(std::fwrite(in, 1, count, handle));
    const BOOL ok = std::fflush(handle) == 0;
#endif
    if (writeBudget > 0) writeBudget -= *written;
    return ok;
}
BOOL testFlushFileBuffers(HANDLE handle) {
    if (flushFailure) { SetLastError(ERROR_DISK_FULL); return FALSE; }
#ifdef _WIN32
    return ::FlushFileBuffers(handle);
#else
    return std::fflush(handle) == 0;
#endif
}
BOOL testCloseHandle(HANDLE handle) {
#ifdef _WIN32
    const BOOL result = ::CloseHandle(handle);
#else
    const BOOL result = std::fclose(handle) == 0;
#endif
    --handles;
    if (closeFailure) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
    return result;
}
#define CreateFile2 testCreateFile2
#define GetFileSizeEx testGetFileSizeEx
#define SetFilePointerEx testSetFilePointerEx
#define ReadFile testReadFile
#define WriteFile testWriteFile
#define FlushFileBuffers testFlushFileBuffers
#define CloseHandle testCloseHandle
#include "io_uwp.hpp"
namespace dusk::mods::catalog {
/*NATIVE_INSTALL_POLICY*/
}
namespace borealis::io {
namespace detail { void release_access(void*) noexcept {} }
bool has_scheme(std::string_view location) { return location.find("://") != std::string_view::npos; }
OpenResult failed_open(Status status, std::string message) { return {.status=status, .message=std::move(message)}; }
Status check(std::string_view location) {
    std::error_code error;
    return std::filesystem::exists(fs_path_from_utf8(location), error) ? Status::Ok : Status::NotFound;
}
/*PRODUCTION_METHODS*/
}
std::vector<std::byte> read_bytes(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    std::vector<char> bytes{std::istreambuf_iterator<char>{input}, {}};
    std::vector<std::byte> result(bytes.size());
    std::memcpy(result.data(), bytes.data(), bytes.size()); return result;
}
int main(int argc, char** argv) {
    assert(argc == 2);
    namespace fs = std::filesystem;
    using namespace borealis::io;
    assert(!dusk::mods::catalog::supports_native_installs());
    const fs::path directory = fs::path{argv[1]} / "LocalState/mods/.downloads";
    fs::create_directories(directory);
    const auto path = directory / fs::path{std::u8string{u8"controller-é★.dusk.part"}};
    const auto name = fs_path_to_string(path);
    std::vector<std::byte> payload(131073);
    for (size_t i=0; i<payload.size(); ++i) payload[i] = static_cast<std::byte>(i % 251);
    assert(!SDL_IOFromFile(name.c_str(), "wb"));
    assert(sdlError.find("datastream") != std::string::npos);
    oldBackendCalls = 0;
    {
        auto result = open(name, File::Mode::Truncate); assert(result.status == Status::Ok);
        auto file = std::move(result.file); shortWrite = 7;
        assert(file.write(payload)); shortWrite = 0;
        assert(file.size() == payload.size()); assert(file.flush()); assert(file.seek(0));
        std::vector<std::byte> read(payload.size()+1);
        assert(file.read(read.data(), read.size()) == payload.size());
        read.resize(payload.size()); assert(read == payload);
        assert(file.close()); assert(file.close());
    }
    assert(oldBackendCalls == 0 && handles == 0 && streams == 0);
    assert(read_bytes(path) == payload);
    {
        auto result = open(name, File::Mode::Append); assert(result.status == Status::Ok);
        assert(result.file.seek(0));
        const std::byte suffix[]{std::byte{1},std::byte{2},std::byte{3}};
        assert(result.file.write(suffix)); assert(result.file.close());
        payload.insert(payload.end(), std::begin(suffix), std::end(suffix));
    }
    assert(read_bytes(path) == payload);
    // Metadata and staged install bytes use the same production writer.
    const auto metadataPath = fs_path_to_string(directory / "resume.json.tmp");
    const std::string metadata = "{\"version\":1,\"validator\":{\"type\":\"etag\",\"value\":\"v1\"}}\n";
    auto metadataFile = open(metadataPath, File::Mode::Truncate); assert(metadataFile.status == Status::Ok);
    assert(metadataFile.file.write({reinterpret_cast<const std::byte*>(metadata.data()),metadata.size()}));
    assert(metadataFile.file.close());
    fs::rename(fs_path_from_utf8(metadataPath), directory / "resume.json");
    const auto staging = directory.parent_path() / ".staging/controller.dusk.part";
    fs::create_directories(staging.parent_path()); fs::copy_file(path, staging);
    assert(read_bytes(staging) == payload);
    {
        auto result = open(name, File::Mode::Truncate); assert(result.status == Status::Ok);
        writeBudget = 5; shortWrite = 3;
        assert(!result.file.write(payload));
        assert(result.file.error().find("Windows error 112") != std::string::npos);
        writeBudget = -1; shortWrite = 0; assert(result.file.close());
        assert(read_bytes(path) == std::vector<std::byte>(payload.begin(),payload.begin()+5));
        auto resume = open(name, File::Mode::Append); assert(resume.status == Status::Ok);
        assert(resume.file.write(std::span<const std::byte>{payload}.subspan(5))); assert(resume.file.close());
        assert(read_bytes(path) == payload);
    }
    {
        auto result = open(name, File::Mode::Truncate); zeroWrite = true;
        assert(!result.file.write(payload)); assert(result.file.error().find("no progress") != std::string::npos);
        zeroWrite = false; assert(result.file.close());
    }
    {
        auto result = open(name, File::Mode::Truncate); assert(result.file.write(payload));
        flushFailure = true; assert(!result.file.close());
        assert(result.file.error().find("flush UWP file") != std::string::npos);
        assert(!result.file); flushFailure = false; assert(result.file.close());
    }
    {
        auto result = open(name, File::Mode::Truncate); closeFailure = true;
        assert(!result.file.close()); assert(result.file.error().find("close UWP file") != std::string::npos);
        closeFailure = false;
    }
    openFailure = ERROR_ACCESS_DENIED;
    auto denied = open(name, File::Mode::Truncate); assert(denied.status == Status::Failed);
    assert(denied.message.find("Windows error 5") != std::string::npos); openFailure = 0;
    bindFailure = true; assert(open(name, File::Mode::Truncate).status == Status::Failed); bindFailure = false;
    seekFailure = true; assert(open(name, File::Mode::Append).status == Status::Failed); seekFailure = false;
    assert(handles == 0 && streams == 0);
    {
        auto first = open(name, File::Mode::Truncate);
        auto second = open(fs_path_to_string(directory / "other.part"), File::Mode::Truncate);
        assert(first.file.write(payload)); assert(second.file.write(payload));
        second.file = std::move(first.file); assert(!first.file); assert(second.file); assert(handles == 1);
    }
    assert(handles == 0 && streams == 0);
    // Read-only access preserves the existing platform backend.
    assert(open(name, File::Mode::Read).status != Status::Ok); assert(oldBackendCalls == 1);
    assert(open("https://invalid.example/file", File::Mode::Truncate).status == Status::Unsupported);
    assert(open("", File::Mode::Truncate).status == Status::NotFound);
    std::cout << "PASS .735: native UWP writes, UTF-8 paths, partial writes, append/resume, metadata/staging, disk-full/zero-progress errors, flush/close failures, move ownership and handle cleanup.\n";
}
'''

with tempfile.TemporaryDirectory(prefix='tpr-735-storage-') as directory:
    work = Path(directory)
    cpp = work / 'storage_735.cpp'
    cpp.write_text(stubs.replace('/*PRODUCTION_METHODS*/',methods).replace('/*NATIVE_INSTALL_POLICY*/',native_policy),encoding='utf-8')
    includes = [library / 'include', library / 'src']
    if os.name == 'nt':
        compiler = shutil.which('cl')
        assert compiler, 'MSVC compiler is required on Windows'
        exe = work / 'storage_735.exe'
        command = [compiler,'/nologo','/std:c++20','/EHsc','/utf-8','/W4','/WX','/D_UWP=1']
        command += [f'/I{p}' for p in includes]
        command += [str(cpp),f'/Fe:{exe}',f'/Fo:{work}/']
    else:
        compiler = shutil.which('g++') or shutil.which('clang++')
        assert compiler, 'A C++20 compiler is required'
        exe = work / 'storage_735'
        command = [compiler,'-std=c++20','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-D_UWP=1']
        command += [f'-I{p}' for p in includes]
        command += [str(cpp),'-o',str(exe)]
    subprocess.run(command,check=True,cwd=work)
    subprocess.run([str(exe),str(work)],check=True,cwd=work)
    # Native installs stay available on desktop platforms.
    desktop_cpp = work / 'desktop_policy.cpp'
    desktop_cpp.write_text('#include <cassert>\n'+native_policy+'\nint main() { assert(supports_native_installs()); }\n')
    desktop_command = [argument for argument in command if argument not in ['/D_UWP=1','-D_UWP=1']]
    desktop_command = [str(desktop_cpp) if argument == str(cpp) else argument for argument in desktop_command]
    subprocess.run(desktop_command,check=True,cwd=work)
    subprocess.run([str(exe)],check=True,cwd=work)
    print('PASS .735: online native installs are blocked on UWP and remain enabled on desktop.')
