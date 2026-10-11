"""Compile the real asset activation branch and check persisted variant selection."""
import argparse,os,re,shutil,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('--negative-control',action='store_true');a=p.parse_args();root=a.source.resolve();control=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='mod-groups-743-') as temp:
    work=Path(temp);name='src/dusk/mods/loader/loader.cpp';source=(root/name).read_text()
    if a.negative_control:
        f=work/name;f.parent.mkdir(parents=True);f.write_text(source)
        subprocess.run(['git','-C',str(work),'apply','--reverse','--include='+name,str(control/'patches/custom-mod-groups-743-host.patch')],check=True);source=f.read_text()
    start=source.index('bool ModLoader::activate_mod(');end=source.index('    if (mod.native && !mod.servicesRegistered)',start)
    (work/'production-group.inc').write_text(source[start:end]+'    return true;\n}\n')
    compiler=shutil.which('cl' if os.name=='nt' else 'c++');binary=work/('groups.exe' if os.name=='nt' else 'groups');fixture=control/'tests/custom_mod_groups.cpp'
    if os.name=='nt':cmd=[compiler,'/nologo','/EHsc','/std:c++20','/MD','/I'+str(work),str(fixture),'/Fe:'+str(binary)]
    else:cmd=[compiler,'-std=c++20','-O0','-I'+str(work),str(fixture),'-o',str(binary)]
    subprocess.run(cmd,cwd=work,check=True);r=subprocess.run([str(binary)]+(['negative'] if a.negative_control else []),cwd=work)
    if a.negative_control:
        if r.returncode!=42:raise RuntimeError('Original loader did not reproduce competing active outfit variants')
        print('PASS: original asset activation allows conflicting variants simultaneously')
    elif r.returncode:raise SystemExit(r.returncode)
