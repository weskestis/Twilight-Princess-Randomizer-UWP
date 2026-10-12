"""Compile and navigate the actual provider with packaged-count image traffic."""
import argparse,importlib.util,os,shutil,subprocess,tempfile
from pathlib import Path
control=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('images742',control/'scripts/test-742-mod-menu-images.py');legacy=importlib.util.module_from_spec(spec);spec.loader.exec_module(legacy)
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('--negative-control',action='store_true');a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='menu-744-') as temp:
    work=Path(temp);stubs=dict(legacy.STUBS)
    loader=stubs['dusk/mod_loader.hpp'].replace('return 12;','return 33;')
    loader=loader.replace('Failure failure = Failure::None;', 'Failure failure = Failure::None; bool prepared=false; std::string lastRead;\n    bool file_exists(const std::string&){return prepared;}')
    loader=loader.replace('readFile(const std::string&)','readFile(const std::string& path)')
    loader=loader.replace('std::vector<uint8_t> data(12);\n        std::memcpy(data.data(), &width, 4);\n        std::memcpy(data.data()+4, &height, 4);\n        data[8]', '''lastRead=path; const auto w=path.starts_with("res/uwp-ui-744/")?128u:width;const auto h=path.starts_with("res/uwp-ui-744/")?128u:height;
        std::vector<uint8_t> data(33);
        const uint8_t prefix[]={137,80,78,71,13,10,26,10,0,0,0,13,'I','H','D','R'};
        std::copy(std::begin(prefix),std::end(prefix),data.begin());
        for(int i=0;i<4;++i){data[16+i]=uint8_t(w>>(24-i*8));data[20+i]=uint8_t(h>>(24-i*8));}
        data[32]''')
    stubs['dusk/mod_loader.hpp']=loader
    for relative,text in stubs.items():
        file=work/relative;file.parent.mkdir(parents=True,exist_ok=True);file.write_text(text)
    production=(a.source/'src/dusk/ui/mod_texture_provider.cpp').read_bytes().replace(b'\r\n',b'\n')
    if a.negative_control:
        file=work/'baseline/src/dusk/ui/mod_texture_provider.cpp';file.parent.mkdir(parents=True);file.write_bytes(production)
        subprocess.run(['git','-C',str(work/'baseline'),'apply','--reverse','--include=src/dusk/ui/mod_texture_provider.cpp',str(control/'patches/xbox-menu-performance-744.patch')],check=True)
        production=file.read_bytes()
        # The .743 provider has no prepared-image helpers; these fixture-only
        # symbols are unused by its early negative-control exit.
        production+=b'\nnamespace dusk::ui {std::string prepared_image_path(std::string_view){return {};} auto& image_use(){return image_cache();}}\n'
    (work/'production-provider.cpp').write_bytes(production)
    compiler=shutil.which('cl' if os.name=='nt' else 'c++');exe=work/('menu.exe' if os.name=='nt' else 'menu')
    fixture=control/'tests/mod_menu_performance.cpp'
    if os.name=='nt':cmd=[compiler,'/nologo','/std:c++20','/EHsc','/MD','/DAURORA_ENABLE_RMLUI=1','/I'+str(work),str(fixture),'/Fe:'+str(exe)]
    else:cmd=[compiler,'-std=c++20','-O1','-DAURORA_ENABLE_RMLUI=1','-I'+str(work),str(fixture),'-o',str(exe)]
    subprocess.run(cmd,cwd=work,check=True);r=subprocess.run([str(exe)]+(['negative'] if a.negative_control else []),cwd=work)
    if a.negative_control:
        assert r.returncode==42,'Shipped .743 icon eviction was not reproduced';print('PASS: shipped .743 evicts list icons before the complete 88-entry shelf fits')
    elif r.returncode:raise SystemExit(r.returncode)
