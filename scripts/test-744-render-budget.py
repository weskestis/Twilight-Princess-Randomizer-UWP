"""Exercise the production tint, effect extent/sample budget and delayed panel policy."""
import argparse,os,shutil,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('source',type=Path);a=p.parse_args();root=a.source.resolve()
cpp=r'''
#include "dusk/breakable_tint.hpp"
#include "dusk/ui/mod_panel_delay.hpp"
#include "xbox_effect_budget.hpp"
#include <cassert>
#include <climits>
#include <cstdio>
struct Color { uint8_t r,g,b,a; };
int main(){
 for(bool pumpkin:{false,true})for(unsigned v=0;v<256;v+=17)for(unsigned alpha:{0u,1u,127u,255u}){
  Color c{uint8_t(v),uint8_t(v),uint8_t(v),uint8_t(alpha)};dusk::breakable_tint::apply(c,pumpkin);
  assert(c.r<=v&&c.g<=v&&c.b<=v&&c.a==alpha);
  if(pumpkin)assert(c.g>=c.r&&c.g>=c.b);else assert(c.r>=c.g&&c.g>=c.b);
 }
 auto e=xbox_effect_budget::fit(1520,1120,720);assert(e.width==977&&e.height==720);
 e=xbox_effect_budget::fit(960,540,720);assert(e.width==960&&e.height==540);
 e=xbox_effect_budget::fit(3040,2240,0);assert(e.width==3040&&e.height==2240);
 e=xbox_effect_budget::fit(UINT_MAX,UINT_MAX,720);assert(e.width==720&&e.height==720);
 assert(xbox_effect_budget::fit(0,480,720).width==0);
 assert(!xbox_effect_budget::balanced(true,1080)&&xbox_effect_budget::balanced(true,2240));
 assert(!xbox_effect_budget::balanced(false,4320));
 float slices=7,steps=3;xbox_effect_budget::bound_ao_samples(slices,steps);assert(slices*steps==10);
 slices=3;steps=1;xbox_effect_budget::bound_ao_samples(slices,steps);assert(slices==3&&steps==1);
 using D=dusk::ui::ModPanelDelay;using namespace std::chrono;D delay;const auto start=D::Clock::time_point{};
 for(int i=0;i<100;++i){delay.select(std::to_string(i),7,1,start+milliseconds(i*3));assert(!delay.take(std::to_string(i),7,1,start+milliseconds(i*3+2)));}
 assert(!delay.take("99",7,1,start+milliseconds(396)));assert(delay.take("99",7,1,start+milliseconds(397)));
 assert(!delay.take("99",7,1,start+seconds(1)));
 delay.select("mod",7,1,start);assert(!delay.take("mod",8,1,start+seconds(1)));
 delay.select("mod",7,1,start);assert(!delay.take("mod",7,2,start+seconds(1)));
 delay.select("mod",7,1,start);delay.cancel();assert(!delay.take("mod",7,1,start+seconds(1)));
 puts("PASS .744: lighting/alpha-preserving tints, DOF aspect/extent budget, AO sample bounds and burst/reload/cancel panel timing");
}
'''
for relative in ['src/d/actor/d_a_obj_carry.cpp','src/d/actor/d_a_obj_pumpkin.cpp']:
    text=(root/relative).read_text();assert 'breakable_tint::apply(' in text
    assert text.index('mDoExt_modelUpdateDL(mpModel);',text.index('breakable_tint::apply('))<text.index('SavedMarkerColor;',text.index('breakable_tint::apply('))
dof=(root/'mods/uwp_ports/port_dof_hq/src/mod.cpp').read_text();ao=(root/'mods/uwp_ports/port_vbao/mods/vbao/src/mod.cpp').read_text()
assert 'xbox_effect_budget::fit(' in dof and '"xboxEffectHeight", 720' in dof
assert 'xbox_effect_budget::bound_ao_samples(' in ao and '"xboxBalanced", true' in ao
assert 'g_targets.fullWidth == fullWidth && g_targets.fullHeight == fullHeight' in ao
window=(root/'src/dusk/ui/mods_window.cpp').read_text();assert 'mPanelDelay.take(' in window and 'if (mModPanelBuilt &&' in window
with tempfile.TemporaryDirectory(prefix='render-744-') as temp:
    work=Path(temp);file=work/'budgets.cpp';file.write_text(cpp);exe=work/('budgets.exe' if os.name=='nt' else 'budgets')
    compiler=shutil.which('cl' if os.name=='nt' else 'c++');includes=[root/'src',root/'mods/uwp_ports']
    if os.name=='nt':cmd=[compiler,'/nologo','/std:c++20','/EHsc',*[('/I'+str(i)) for i in includes],str(file),'/Fe:'+str(exe)]
    else:cmd=[compiler,'-std=c++20','-O1',*[('-I'+str(i)) for i in includes],str(file),'-o',str(exe)]
    subprocess.run(cmd,cwd=work,check=True);subprocess.run([str(exe)],check=True)
