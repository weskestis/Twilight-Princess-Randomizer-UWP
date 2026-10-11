"""Exercise production installer path validation with denied ancestor canonicalization."""
import argparse,os,shutil,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('--negative-control',action='store_true');a=p.parse_args();root=a.source.resolve();control=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='install743-') as temp:
    work=Path(temp);src=(root/'src/dusk/mods/loader/loader.cpp').read_text()
    if a.negative_control:
        original=work/'old/src/dusk/mods/loader/loader.cpp';original.parent.mkdir(parents=True);original.write_text(src)
        header=work/'old/src/dusk/mods/install_paths.hpp';shutil.copyfile(root/'src/dusk/mods/install_paths.hpp',header)
        subprocess.run(['git','-C',str(work/'old'),'apply','--reverse',str(control/'patches/xbox-mod-install-paths-743-host.patch')],check=True)
        src=original.read_text()
    start=src.index('ModLoader::OperationResult ModLoader::install_staged(')
    end=src.index('    ModMetadata metadata;',start)
    prefix=src[start:end]+'    return {.success=true};\n}\n'
    (work/'production-install-paths.inc').write_text(prefix)
    cc=shutil.which('cl' if os.name=='nt' else 'g++');exe=work/('proof.exe' if os.name=='nt' else 'proof')
    fixture=control/'tests/mod_install_paths.cpp'
    cmd=[cc,'/nologo','/std:c++20','/EHsc','/D_UWP=1','/I'+str(root/'src'),'/I'+str(work),str(fixture),'/Fe:'+str(exe)] if os.name=='nt' else [cc,'-std=c++20','-O2','-D_UWP=1','-I'+str(root/'src'),'-I'+str(work),str(fixture),'-o',str(exe)]
    subprocess.run(cmd,cwd=work,check=True)
    result=subprocess.run([str(exe),str(work/'app')]+(['negative'] if a.negative_control else []))
    if result.returncode!=(42 if a.negative_control else 0):raise RuntimeError('Path proof returned '+str(result.returncode))
    print('PASS: .742 reproduces access-denied canonicalization' if a.negative_control else 'PASS: UWP private staging accepted; outside/relative/missing/wrong-type/symlink escapes rejected')
