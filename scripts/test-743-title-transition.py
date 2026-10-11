"""Test the production title transition against delayed overlap publication and retries."""
import argparse,os,re,shutil,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('--negative-control',action='store_true');a=p.parse_args();control=Path(__file__).resolve().parents[1]
def function(source,signature):
    start=source.index(signature);depth=0
    for m in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/|[{}]',source[start:]):
        if m.group()=='{':depth+=1
        elif m.group()=='}':
            depth-=1
            if not depth:return source[start:start+m.end()]
    raise RuntimeError('Unclosed '+signature)
with tempfile.TemporaryDirectory(prefix='title-743-') as temp:
    work=Path(temp);name='src/d/actor/d_a_title.cpp';source=(a.source.resolve()/name).read_text()
    if a.negative_control:
        f=work/name;f.parent.mkdir(parents=True);f.write_text(source)
        subprocess.run(['git','-C',str(work),'apply','--reverse',str(control/'patches/xbox-title-transition-743-host.patch')],check=True);source=f.read_text()
    body='const daTitle_c* s_xboxTitleTransitionOwner = nullptr;\nbool s_xboxTitleTransitionAccepted = false;\n'
    body+=function(source,'void daTitle_c::nextScene_init(')+'\n'+function(source,'void daTitle_c::nextScene_proc(')
    (work/'production-title.inc').write_text(body);compiler=shutil.which('cl' if os.name=='nt' else 'c++');binary=work/('title.exe' if os.name=='nt' else 'title');fixture=control/'tests/title_transition.cpp'
    if os.name=='nt':cmd=[compiler,'/nologo','/EHsc','/std:c++20','/MD','/D_UWP=1','/DDEBUG=0','/DVERSION=1','/DVERSION_SHIELD_DEBUG=999','/I'+str(work),str(fixture),'/Fe:'+str(binary)]
    else:cmd=[compiler,'-std=c++20','-D_UWP=1','-DDEBUG=0','-DVERSION=1','-DVERSION_SHIELD_DEBUG=999','-I'+str(work),str(fixture),'-o',str(binary)]
    subprocess.run(cmd,cwd=work,check=True);r=subprocess.run([str(binary)]+(['negative'] if a.negative_control else []),cwd=work)
    if a.negative_control:
        if r.returncode!=42:raise RuntimeError('Original title did not repeatedly submit accepted scene requests')
        print('PASS: original title reissues scene request while overlap publication is delayed')
    elif r.returncode:raise SystemExit(r.returncode)
