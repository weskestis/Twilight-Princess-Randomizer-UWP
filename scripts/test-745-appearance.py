"""Exercise the actual cosmetics runtime and verify scene/getter integration."""
import argparse,os,shutil,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('source',type=Path);a=p.parse_args();r=a.source.resolve();c=Path(__file__).resolve().parents[1]
mod=(r/'mods/cosmetics/src/mod.cpp').read_text();host=(r/'src/dusk/ui/menu_colors.cpp').read_text()
assert 'gAppearanceRuntime.string(handle)' in mod and 'gAppearanceRuntime.integer(handle)' in mod
assert mod.index('sync_xbox_appearance();',mod.index('MOD_EXPORT ModResult mod_update')) < mod.index('set_all_midna_hair_colors();',mod.index('MOD_EXPORT ModResult mod_update'))
assert 'gAppearanceRuntime.clear();' in mod and 'tpr_appearance_set_mode' in mod
assert 'tpr_appearance_seed()' in host and 'sAppearanceRevision = value;' in host
assert 'randomize_menu_colors_for_load_screen();' in (r/'src/d/d_s_play.cpp').read_text()
assert 'Randomize All Colors on Every Load Screen' in mod and 'Randomize All Colors on Every Load Screen' in host
for name in ['herosTunicCapColor','zoraArmorHelmetColor','lanternGlowColor','msBladeColor','boomerangColor','ironBootsColor','spinnerColor','aButtonColor','bButtonColor','xButtonColor','yButtonColor','zButtonColor','heartColor','midnaHairBaseColor','midnaHairTipsColor','midnaChargeRingColor','linkHairColor','wolfLinkColor','eponaColor']: assert '{g_cvars.'+name+',' in mod,name
with tempfile.TemporaryDirectory(prefix='appearance-745-') as tmp:
 w=Path(tmp);exe=w/('appearance.exe' if os.name=='nt' else 'appearance');include=r/'mods/cosmetics/src'
 if os.name=='nt':cmd=['cl','/nologo','/EHsc','/std:c++20','/I'+str(include),str(c/'tests/appearance_restore_745.cpp'),'/Fe:'+str(exe)]
 else:cmd=['c++','-std=c++20','-O1','-I'+str(include),str(c/'tests/appearance_restore_745.cpp'),'-o',str(exe)]
 subprocess.run(cmd,cwd=w,check=True);subprocess.run([str(exe)],check=True)
 # Exercise the actual authored nine-register Midna palette and interpolator.
 source=(r/'mods/cosmetics/src/midna_hair_color.cpp').read_text()
 colors=source[source.index('constexpr std::array<std::array<GXColor, 9>, 10>'):source.index('GXColor xbox_blend_midna_color')]
 functions=source[source.index('GXColor xbox_blend_midna_color'):source.index('size_t get_midna_hair_color_index')]
 cpp='''#include <array>\n#include <cassert>\n#include <cstdint>\n#include <cstdio>\nusing u8=uint8_t;using ConfigVarHandle=uint64_t;\nstruct GXColor {u8 r,g,b,a=0;};\nbool enabled=false;uint64_t elapsed=0;\nbool get_xbox_midna_rainbow_phase(ConfigVarHandle,uint64_t& ms,uint64_t& duration){ms=elapsed;duration=2000;return enabled;}\n'''+colors+functions+'''
int main(){
 for(size_t choice=0;choice<10;++choice)for(size_t i=0;i<9;++i){auto a=xbox_selected_midna_color(1,choice,i);auto b=kMidnaHairColors[choice][i];assert(a.r==b.r&&a.g==b.g&&a.b==b.b&&a.a==b.a);}
 enabled=true;
 for(size_t i=0;i<9;++i){
  elapsed=0;auto red=xbox_selected_midna_color(1,0,i);assert(red.r==kMidnaHairColors[2][i].r);
  elapsed=2000;auto orange=xbox_selected_midna_color(1,0,i);auto expect=xbox_blend_midna_color(kMidnaHairColors[2][i],kMidnaHairColors[3][i],1,2);assert(orange.r==expect.r&&orange.g==expect.g&&orange.b==expect.b);
  elapsed=1000;auto fade=xbox_selected_midna_color(1,0,i);expect=xbox_blend_midna_color(red,orange,1,2);assert(fade.r==expect.r&&fade.g==expect.g&&fade.b==expect.b);
  elapsed=8000;auto cyan=xbox_selected_midna_color(1,0,i);expect=xbox_blend_midna_color(kMidnaHairColors[4][i],kMidnaHairColors[5][i],1,2);assert(cyan.r==expect.r&&cyan.g==expect.g&&cyan.b==expect.b);
  elapsed=16000;auto loop=xbox_selected_midna_color(1,0,i);assert(loop.r==red.r&&loop.g==red.g&&loop.b==red.b);
 }
 puts("PASS: original Midna palette choices and continuous nine-register red/orange/cyan Rainbow interpolation");
}
'''
 f=w/'midna.cpp';f.write_text(cpp);midna=w/('midna.exe' if os.name=='nt' else 'midna')
 cmd=['cl','/nologo','/EHsc','/std:c++20','/D_UWP=1',str(f),'/Fe:'+str(midna)] if os.name=='nt' else ['c++','-std=c++20','-D_UWP=1','-O1',str(f),'-o',str(midna)]
 subprocess.run(cmd,cwd=w,check=True);subprocess.run([str(midna)],check=True)
