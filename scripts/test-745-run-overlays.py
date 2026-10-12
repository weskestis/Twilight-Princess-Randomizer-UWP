"""Compile the complete shipped tracker and restored overlays with real ImGui."""
import argparse,importlib.util,os,shutil,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('--feed',type=Path);p.add_argument('--imgui',type=Path);p.add_argument('--fmt',type=Path);a=p.parse_args();r=a.source.resolve();c=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('tracker_test',c/'scripts/test-742-pure-tracker-position.py');old=importlib.util.module_from_spec(spec);spec.loader.exec_module(old)
imgui=(a.imgui or a.feed/'_deps/imgui-src').resolve();fmt=(a.fmt or a.feed/'_deps/fmt-src/include').resolve()
assert 'v1.91.9b' in (imgui/'imgui.h').read_text()[:150]
stubs=dict(old.STUBS);fields=''
for prefix,y in [('deathCounter','0.054f'),('gameTimer','0.015f')]:
 fields+='        TestVar<bool> show'+prefix[0].upper()+prefix[1:]+'Overlay{false};\n'
 for suffix,typ,value in [('Scale','float','1.25f'),('PositionX','float','0.5f'),('PositionY','float',y),('Lock','bool','false'),('ResetLayoutRequested','bool','false')]:fields+='        TestVar<'+typ+'> '+prefix+'Overlay'+suffix+'{'+value+'};\n'
stubs['dusk/settings.h']=stubs['dusk/settings.h'].replace('        TestVar<bool> showPureItemTracker',fields+'        TestVar<bool> showPureItemTracker').replace('namespace dusk {','namespace dusk {\ntemplate<class T> using ConfigVar = TestVar<T>;',1)
# Alias must follow the fixture class definition.
stubs['dusk/settings.h']=stubs['dusk/settings.h'].replace('template<class T> using ConfigVar = TestVar<T>;','').replace('struct TestSettings {','template<class T> using ConfigVar = TestVar<T>;\nstruct TestSettings {')
stubs['d/d_com_inf_game.h']='#pragma once\ninline unsigned short dComIfGs_getDeathCount() { return 17; }\n'
assert 'draw_run_overlays();' in (r/'src/dusk/imgui/ImGuiConsole.cpp').read_text()
for prefix in ['deathCounter','gameTimer']:
 for suffix in ['Scale','PositionX','PositionY','Lock','ResetLayoutRequested']:
  name=prefix+'Overlay'+suffix
  assert name in (r/'src/dusk/settings.h').read_text() and 'Register(g_userSettings.game.'+name+');' in (r/'src/dusk/settings.cpp').read_text()
with tempfile.TemporaryDirectory(prefix='overlays-745-') as tmp:
 w=Path(tmp)
 for name,content in stubs.items():f=w/name;f.parent.mkdir(parents=True,exist_ok=True);f.write_text(content)
 (w/'pure_tracker_position.cpp').write_text((c/'tests/pure_tracker_position.cpp').read_text().replace('int main() {','void main() {'))
 exe=w/('overlays.exe' if os.name=='nt' else 'overlays')
 sources=[r/'src/dusk/imgui/PureItemTracker.cpp',c/'tests/run_overlays_745.cpp']+[imgui/n for n in ['imgui.cpp','imgui_draw.cpp','imgui_tables.cpp','imgui_widgets.cpp']]
 includes=[w,imgui,fmt,r/'src/dusk/imgui',c/'tests']
 if os.name=='nt':cmd=['cl','/nologo','/EHsc','/std:c++20','/MD','/D_UWP=1','/DFMT_HEADER_ONLY=1',*['/I'+str(i) for i in includes],*map(str,sources),'/Fe:'+str(exe)]
 else:cmd=['c++','-std=c++20','-D_UWP=1','-DFMT_HEADER_ONLY=1','-O0',*['-I'+str(i) for i in includes],*map(str,sources),'-o',str(exe)]
 subprocess.run(cmd,cwd=w,check=True);subprocess.run([str(exe)],check=True)
