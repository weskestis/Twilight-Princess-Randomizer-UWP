"""Run the author's unchanged 33 core tests against the built native dependencies."""
import argparse
import json
import os
import re
import subprocess
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('source', type=Path)
p.add_argument('feed', type=Path)
p.add_argument('output', type=Path)
a = p.parse_args()
control = Path(__file__).resolve().parents[1]
mod = next(m for m in json.loads((control / 'NATIVE_MODS-744.json').read_text())['mods'] if m['target'] == 'port_mfb_multiplayer')
author = a.source.resolve() / '.uwp-native-source-744' / mod['target']
assert subprocess.check_output(['git', '-C', str(author), 'rev-parse', 'HEAD'], text=True).strip() == mod['sourceCommit']
text = (author / 'CMakeLists.txt').read_text()
body = text[text.index('option(DUSKLIGHT_ONLINE_BUILD_TESTS'):]
# The source filenames and test bodies remain the author's exact pinned files.
# Only their CMake paths change so no additional host or desktop mod is built.
body = body.replace('${CMAKE_CURRENT_SOURCE_DIR}', '${AUTHOR_SOURCE}')
body = re.sub(r'(?<![\w/$])((?:src|tests|vendor|tools)/[\w./-]+)', r'"${AUTHOR_SOURCE}/\1"', body)
variables = []
for name in ['ONLINE_INVITE_SOURCES', 'ONLINE_CONNECTION_SOURCES', 'ONLINE_CORE_SOURCES']:
    match = re.search(r'set\(' + name + r'\n.*?\n\)', text, re.S)
    assert match, 'Author source list changed: ' + name
    block = re.sub(r'(?<![\w/$])((?:src|vendor)/[\w./-]+)', r'"${AUTHOR_SOURCE}/\1"', match.group())
    variables.append(block)
feed = a.feed.resolve()

def library(names):
    files = [file for name in names for file in feed.rglob(name)]
    assert len(files) == 1, 'Native dependency lookup is ambiguous: ' + repr(files)
    return files[0].as_posix()

juice = library(['juice-static.lib'] if os.name == 'nt' else ['libjuice-static.a'])
zstd = library(['zstd_static.lib', 'libzstd_static.lib'] if os.name == 'nt' else ['libzstd.a'])
dependency_roots = [feed / '_deps']
for line in (feed / 'CMakeCache.txt').read_text().splitlines():
    if line.startswith('FETCHCONTENT_SOURCE_DIR_') and '=' in line:
        value = line.split('=', 1)[1]
        if value:
            dependency_roots.append(Path(value))
juice_header = next(file for path in dependency_roots for file in path.rglob('juice.h'))
juice_include = juice_header.parent.parent.as_posix()
json_header = next(file for path in dependency_roots for file in path.rglob('json.hpp') if file.parent.name == 'nlohmann')
zstd_header = next(file for path in dependency_roots for file in path.rglob('zstd.h'))
assert json_header.parent.name == 'nlohmann'
json_include = json_header.parent.parent.as_posix()
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=True)
project = out / 'source'
project.mkdir(exist_ok=True)
prefix = '''cmake_minimum_required(VERSION 3.26)
project(mfb_author_tests LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE Release)
set(DUSKLIGHT_ONLINE_BUILD_TESTS ON)
find_package(Threads REQUIRED)
add_library(juice-static STATIC IMPORTED)
add_library(zstd_native STATIC IMPORTED)
add_library(json_headers INTERFACE)
add_library(nlohmann_json::nlohmann_json ALIAS json_headers)
set(ONLINE_ZSTD_TARGET zstd_native)
add_compile_definitions(JUICE_STATIC)
link_libraries(Threads::Threads juice-static)
if (WIN32)
    link_libraries(ws2_32 bcrypt)
endif ()
'''
prefix += 'set(AUTHOR_SOURCE "' + author.as_posix() + '")\n'
prefix += 'set_target_properties(juice-static PROPERTIES IMPORTED_LOCATION "' + juice + '")\n'
prefix += 'set_target_properties(zstd_native PROPERTIES IMPORTED_LOCATION "' + zstd + '")\n'
prefix += 'target_include_directories(zstd_native INTERFACE "' + zstd_header.parent.as_posix() + '")\n'
prefix += 'target_include_directories(json_headers INTERFACE "' + json_include + '")\n'
prefix += 'include_directories("${AUTHOR_SOURCE}/include" "${AUTHOR_SOURCE}/src" "${AUTHOR_SOURCE}/vendor/kcp" "${AUTHOR_SOURCE}/vendor/monocypher" "' + juice_include + '")\n'
(project / 'CMakeLists.txt').write_text(prefix + '\n'.join(variables) + '\n' + body, encoding='utf-8', newline='\n')
build = out / 'build'
subprocess.run(['cmake', '-S', str(project), '-B', str(build), '-G', 'Ninja'], check=True)
subprocess.run(['cmake', '--build', str(build), '--parallel', '3'], check=True)
result = subprocess.run(['ctest', '--test-dir', str(build), '--output-on-failure', '--output-junit', str(out / 'mfb-author-tests.xml'), '-j', '2'], capture_output=True, text=True)
(out / 'mfb-author-tests.txt').write_text(result.stdout + result.stderr)
print(result.stdout)
raise SystemExit(result.returncode)
