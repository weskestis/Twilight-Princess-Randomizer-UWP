"""Compile production controller panel/callbacks and exercise late selection safely."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('source', type=Path)
parser.add_argument('--negative-control', action='store_true')
args = parser.parse_args()
args.source = args.source.resolve()
control = Path(__file__).resolve().parents[1]
text = (args.source/'mods/controller_ui/src/mod.cpp').read_text(encoding='utf-8')

def block(start, end):
    return text[text.index(start):text.index(end, text.index(start))]

with tempfile.TemporaryDirectory(prefix='controller743-') as directory:
    work = Path(directory)
    if args.negative_control:
        p=work/'original/mods/controller_ui/src/mod.cpp'; p.parent.mkdir(parents=True)
        p.write_text(text, encoding='utf-8')
        subprocess.run(['git','-C',str(work/'original'),'apply','--reverse',str(control/'patches/xbox-controller-ui-selection-743.patch')],check=True)
        text=p.read_text(encoding='utf-8')
    table=block('struct ControllerTexture {','// Faces are resolved')
    buffers=sorted(set(re.findall(r'&s_[a-z_0-9]+',table)))
    generated=''.join('ResourceBuffer '+name[1:]+'=RESOURCE_BUFFER_INIT;\n' for name in buffers)
    generated+=block('int64_t raw_controller_style()', 'ConfigVarHandle switch_variant(')
    selected='bool controller_style_locked(' if args.negative_control else 'bool playstation_selected('
    generated+=block(selected, 'static int layout_preset_index(')
    generated+=block('ModResult build_layout_panel(', 'ModResult build_public_general_panel(')
    generated+=table
    (work/'production-controller.inc').write_text(generated,encoding='utf-8')
    compiler=shutil.which('cl') if os.name=='nt' else shutil.which('g++')
    exe=work/('controller.exe' if os.name=='nt' else 'controller')
    include=args.source/'sdk/include'
    fixture=control/'tests/controller_ui_selection.cpp'
    cmd=[compiler,'/nologo','/std:c++20','/EHsc','/I'+str(include),'/I'+str(work),str(fixture),'/Fe:'+str(exe)] if os.name=='nt' else [compiler,'-std=c++20','-O2','-Wall','-Wextra','-I'+str(include),'-I'+str(work),str(fixture),'-o',str(exe)]
    subprocess.run(cmd,cwd=work,check=True)
    result=subprocess.run([str(exe)]+(['negative'] if args.negative_control else []),cwd=work)
    expected=23 if args.negative_control else 0
    if result.returncode!=expected: raise RuntimeError('Controller selection proof returned '+str(result.returncode))
    print('PASS: .742 negative control rejects Xbox selection after first prompt' if args.negative_control else 'PASS: production panel saves every design during play; active textures and calibration stay paired until restart')
