"""Compile the actual Aurora and mod SDK headers against the pinned WebGPU SDK.

This checks the engine/SDK integration before the lengthy game build. The ABI
fixture separately exercises old clients; it cannot detect duplicate members
inside the production Aurora header when using its own engine test doubles.
"""
import argparse, os, shutil, subprocess, tempfile
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('source', type=Path)
p.add_argument('webgpu_include', type=Path)
a = p.parse_args()
with tempfile.TemporaryDirectory(prefix='gfx-headers-754-') as directory:
    work = Path(directory)
    source = work / 'headers.cpp'
    source.write_text('''#define DUSK_MOD_FEATURE_WEBGPU 1
#include <aurora/gfx.hpp>
#include <mods/svc/gfx.h>
#include <type_traits>
using Targets = aurora::gfx::ResolvedTargets;
static_assert(std::is_same_v<decltype(Targets::colorTexture), wgpu::Texture>);
static_assert(std::is_same_v<decltype(Targets::depthTexture), wgpu::Texture>);
static_assert(std::is_same_v<decltype(Targets::normalTexture), wgpu::Texture>);
''')
    includes = [a.source / 'extern/aurora/include', a.source / 'sdk/include', a.webgpu_include]
    compiler = shutil.which('cl' if os.name == 'nt' else 'c++')
    if os.name == 'nt':
        command = [compiler, '/nologo', '/std:c++20', '/EHsc', '/c', '/Fo:' + str(work / 'headers.obj')]
        command += ['/I' + str(path.resolve()) for path in includes]
    else:
        command = [compiler, '-std=c++20', '-fsyntax-only']
        command += ['-I' + str(path.resolve()) for path in includes]
    subprocess.run(command + [str(source)], check=True, cwd=work)
print('PASS: actual Aurora and mod SDK graphics headers compile with the pinned WebGPU SDK')
