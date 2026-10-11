"""Compile the real snapshot adapter and preserve older caller buffer sizes."""
import argparse, os, re, shutil, subprocess, tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('webgpu_include',type=Path);a=p.parse_args()
control=Path(__file__).resolve().parents[1];source=a.source.resolve()
def function(text,signature):
    start=text.index(signature);depth=0
    for m in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/|[{}]',text[start:]):
        if m.group()=='{':depth+=1
        elif m.group()=='}':
            depth-=1
            if not depth:return text[start:start+m.end()]
    raise RuntimeError('Unclosed graphics function')
text=(source/'src/dusk/mods/svc/gfx.cpp').read_text(encoding='utf-8')
recording=(source/'extern/aurora/lib/gfx/recording.cpp').read_text(encoding='utf-8')
for kind in ['color','depth','normal']:
    assert 'out.'+kind+'Texture = entry.'+kind+'.texture;' in recording
assert recording.count('TextureUsage::CopySrc | wgpu::TextureUsage::TextureBinding,')>=3
with tempfile.TemporaryDirectory(prefix='gfx-743-') as n:
    work=Path(n);(work/'production-gfx.inc').write_text(function(text,'ModResult gfx_resolve_pass(')+'\n'+function(text,'ModResult gfx_resolve_pass_impl(')+'\n',encoding='utf-8')
    compiler=shutil.which('cl' if os.name=='nt' else 'g++');exe=work/('gfx.exe' if os.name=='nt' else 'gfx')
    includes=[work,source/'sdk/include',a.webgpu_include.resolve()];fixture=control/'tests/gfx_snapshot_abi.cpp'
    if os.name=='nt':cmd=[compiler,'/nologo','/std:c++20','/EHsc','/MD','/DDUSK_MOD_FEATURE_WEBGPU=1',*['/I'+str(i) for i in includes],str(fixture),'/Fe:'+str(exe)]
    else:cmd=[compiler,'-std=c++20','-DDUSK_MOD_FEATURE_WEBGPU=1',*['-I'+str(i) for i in includes],str(fixture),'-o',str(exe)]
    subprocess.run(cmd,cwd=work,check=True);subprocess.run([str(exe)],check=True,cwd=work)
