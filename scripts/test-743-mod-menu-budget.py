"""Exercise the production provider and assert oversized images never reach decoding."""
import argparse,importlib.util,os,shutil,subprocess,tempfile
from pathlib import Path
control=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('previous_images',control/'scripts/test-742-mod-menu-images.py')
previous=importlib.util.module_from_spec(spec);spec.loader.exec_module(previous)
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('--negative-control',action='store_true');a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='mod-menu-743-') as name:
    work=Path(name);stubs=dict(previous.STUBS)
    text=stubs['dusk/mod_loader.hpp'].replace('return 12;','return 33;')
    text=text.replace('std::vector<uint8_t> data(12);\n        std::memcpy(data.data(), &width, 4);\n        std::memcpy(data.data()+4, &height, 4);\n        data[8]', '''std::vector<uint8_t> data(33);
        const uint8_t prefix[] = {137,80,78,71,13,10,26,10,0,0,0,13,'I','H','D','R'};
        std::copy(std::begin(prefix),std::end(prefix),data.begin());
        for(int i=0;i<4;++i){data[16+i]=uint8_t(width>>(24-i*8));data[20+i]=uint8_t(height>>(24-i*8));}
        data[32]''')
    stubs['dusk/mod_loader.hpp']=text
    for name,text in stubs.items():
        f=work/name;f.parent.mkdir(parents=True,exist_ok=True);f.write_text(text)
    source=a.source.resolve()/'src/dusk/ui/mod_texture_provider.cpp'
    production=source.read_bytes().replace(b'\r\n',b'\n')
    if a.negative_control:
        root=work/'reverse';f=root/'src/dusk/ui/mod_texture_provider.cpp';f.parent.mkdir(parents=True);f.write_bytes(production)
        subprocess.run(['git','-C',str(root),'apply','--reverse',str(control/'patches/xbox-mod-menu-budget-743-host.patch')],check=True)
        production=f.read_bytes()
    (work/'production-provider.cpp').write_bytes(production)
    compiler=shutil.which('cl' if os.name=='nt' else 'c++');binary=work/('images.exe' if os.name=='nt' else 'images')
    fixture=control/'tests/mod_menu_budget.cpp'
    if os.name=='nt':cmd=[compiler,'/nologo','/EHsc','/std:c++20','/MD','/DAURORA_ENABLE_RMLUI=1','/I'+str(work),str(fixture),'/Fe:'+str(binary)]
    else:cmd=[compiler,'-std=c++20','-O0','-DAURORA_ENABLE_RMLUI=1','-I'+str(work),str(fixture),'-o',str(binary)]
    subprocess.run(cmd,cwd=work,check=True);r=subprocess.run([str(binary)]+(['negative'] if a.negative_control else []),cwd=work)
    if a.negative_control:
        if r.returncode!=42:raise RuntimeError('Shipped .742 did not reach full-size decoding for an over-budget image')
        print('PASS: shipped .742 reaches 64 MiB temporary decode before thumbnail resizing')
    elif r.returncode:raise SystemExit(r.returncode)
