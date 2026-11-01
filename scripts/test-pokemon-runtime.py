"""Run portable model tests and compile the native actor against .749's real headers."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def source_function(source, signature):
    start=source.index(signature)
    opening=source.index('{',start)
    depth=1
    end=opening+1
    while depth:
        if source[end]=='{': depth+=1
        elif source[end]=='}': depth-=1
        end+=1
    return source[start:end]


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--dusk',type=Path,required=True)
    p.add_argument('--pack',type=Path,action='append',default=[])
    p.add_argument('--sanitize',action='store_true')
    args=p.parse_args()
    root=Path(__file__).resolve().parents[1]/'experimental/pokemon_battle'
    dusk=args.dusk.resolve()
    windows=os.name=='nt'
    compiler=shutil.which('cl' if windows else 'g++')
    if not compiler or not (dusk/'extern/aurora/include/dolphin/gx.h').exists():
        p.error('Compiler and pinned Dusklight checkout with Aurora headers required')
    with tempfile.TemporaryDirectory(prefix='pokemon-runtime-') as temp:
        build=Path(temp)
        cmd=([compiler,'/nologo','/EHsc','/std:c++20','/W4','/WX','/utf-8'] if windows else
             [compiler,'-std=c++20','-Wall','-Wextra','-Wpedantic','-Werror'])
        if args.sanitize:
            if windows:p.error('Sanitizers require GCC in this runner')
            cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g']
        exe=build/('runtime-tests.exe' if windows else 'runtime-tests')
        subprocess.run(cmd+[str(root/'runtime/model.cpp'),str(root/'tests/runtime_tests.cpp')]+
                       (['/Fe:'+str(exe)] if windows else ['-o',str(exe)]),cwd=build,check=True)
        subprocess.run([str(exe)]+[str(x.resolve()) for x in args.pack],check=True)
        exe=build/('animation-tests.exe' if windows else 'animation-tests')
        subprocess.run(cmd+[str(root/path) for path in ['runtime/model.cpp','field/party.cpp','field/combat.cpp',
                       'field/animation.cpp','tests/animation_tests.cpp']]+
                       (['/Fe:'+str(exe)] if windows else ['-o',str(exe)]),cwd=build,check=True)
        subprocess.run([str(exe)],check=True)
        # Actual SDK declarations; no substitute game headers or copied layouts.
        include=['include','assets/GZ2E01','libs/JSystem/include',
                 'extern/aurora/include/dolphin','extern/aurora/include','sdk/include']
        native=cmd.copy()
        for path in include:native+=(['/external:I'+str(dusk/path),'/external:W0'] if windows else ['-isystem',str(dusk/path)])
        definitions=['TARGET_PC=1','WIDESCREEN_SUPPORT=1','AVOID_UB=1','VERSION=0',
                     'MTX_USE_PS=1','PARTIAL_DEBUG=1','DUSK_MOD_FEATURE_GAME=1']
        native += [('/D' if windows else '-D')+d for d in definitions]
        native += ['/FIglobal.h'] if windows else ['-include','global.h','-Wno-attributes']
        for path in ['model_lab/gx_renderer.cpp','model_lab/mod.cpp','model_lab/field.cpp','model_lab/field_effects.cpp']:
            subprocess.run(native+(['/Zs'] if windows else ['-fsyntax-only'])+[str(root/path)],cwd=build,check=True)
        # Link the production GX renderer to a recording GX host. This checks actual
        # draw submissions and texture lifetimes; it does not pretend to run a GPU.
        exe=build/('gx-tests.exe' if windows else 'gx-tests')
        subprocess.run(native+[str(root/'runtime/model.cpp'),str(root/'model_lab/gx_renderer.cpp'),
                              str(root/'model_lab/field_effects.cpp'),
                              str(root/'tests/gx_tests.cpp')]+(['/Fe:'+str(exe)] if windows else ['-o',str(exe)]),cwd=build,check=True)
        subprocess.run([str(exe)]+[str(x.resolve()) for x in args.pack],check=True)
        # Compile the unchanged packet insertion/traversal functions from the
        # pinned game, with only their profiling macro disabled in this host.
        draw_source=(dusk/'libs/JSystem/src/J3DGraphBase/J3DDrawBuffer.cpp').read_text()
        draw=build/'real_draw_queue.cpp'
        draw.write_text('#include "JSystem/J3DGraphBase/J3DDrawBuffer.h"\n'
                        '#include "JSystem/J3DGraphBase/J3DPacket.h"\n'
                        '#define ZoneScoped\n'+
                        source_function(draw_source,'int J3DDrawBuffer::entryImm(')+'\n'+
                        source_function(draw_source,'void J3DDrawBuffer::drawHead()')+'\n')
        exe=build/('spawn-tests.exe' if windows else 'spawn-tests')
        spawn_native=native+[('/D' if windows else '-D')+'DUSK_STATIC_BUILTIN_MOD=1']
        subprocess.run(spawn_native+[str(root/'tests/spawn_context_tests.cpp'),str(draw)]+
                       (['/Fe:'+str(exe)] if windows else ['-o',str(exe)]),cwd=build,check=True)
        subprocess.run([str(exe)],check=True)
        print('PASS: runtime parsing/sampling, recorded GX draws, native .749 headers, and actual draw-queue spawn regression')


if __name__=='__main__':main()
