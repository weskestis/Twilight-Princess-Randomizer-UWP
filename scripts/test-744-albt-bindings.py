"""Link the production ALBT particle bridge and protect Windows CRT imports."""
import os
import re
import subprocess
import tempfile
from pathlib import Path

control = Path(__file__).resolve().parents[1]
original = (control / 'native_ports/albt/src/albw_gameinfo_compat.cpp').read_text()
matches = list(re.finditer(r'JPABaseEmitter\s*\*\s*dComIfGp_particle_set\s*\(([^{}]+)\)\s*\{', original))
match = next(m for m in matches if len(m.group(1).split(',')) == 11)
begin = match.end() - 1
depth, end = 1, begin + 1
while depth:
    depth += (original[end] == '{') - (original[end] == '}')
    end += 1
production = original[match.start():end]
# Match the pinned host declaration independently of the implementation.
prototype = '''JPABaseEmitter* dComIfGp_particle_set(u16, const cXyz*,
    const dKy_tevstr_c*, const csXyz*, const cXyz*, u8, dPa_levelEcallBack*,
    s8, const GXColor*, const GXColor*, const cXyz*);\n'''
prefix = r'''
#include "native_ports/albt/src/xbox_private_aliases.hpp"
#ifdef stricmp
#error Windows CRT imports must keep their runtime names
#endif
using u16 = unsigned short; using u8 = unsigned char; using s8 = signed char;
struct cXyz {}; struct csXyz {}; struct GXColor {};
class dKy_tevstr_c {}; class dPa_levelEcallBack {}; class JPABaseEmitter {};
struct Particle {
    u16 id; const cXyz *position, *scale, *particleScale;
    const csXyz *rotation; const dKy_tevstr_c *tev;
    const GXColor *primary, *environment; u8 alpha; s8 room;
    dPa_levelEcallBack *callback; float rate; JPABaseEmitter result;
    JPABaseEmitter* setNormal(u16 a, const cXyz* b, const dKy_tevstr_c* c,
        const csXyz* d, const cXyz* e, u8 f, dPa_levelEcallBack* g, s8 h,
        const GXColor* i, const GXColor* j, const cXyz* k, float l) {
        id=a; position=b; tev=c; rotation=d; scale=e; alpha=f; callback=g;
        room=h; primary=i; environment=j; particleScale=k; rate=l;
        return &result;
    }
};
struct Play { Particle particle; Particle* getParticle() { return &particle; } };
struct Game { Play play; };
extern Game g_dComIfG_gameInfo;
'''
caller = r'''
Game g_dComIfG_gameInfo;
int main() {
    cXyz position, scale, particleScale; csXyz rotation;
    dKy_tevstr_c tev; dPa_levelEcallBack callback; GXColor primary, environment;
    auto* result = dComIfGp_particle_set(0x85af, &position, &tev, &rotation,
        &scale, 173, &callback, -7, &primary, &environment, &particleScale);
    const auto& p = g_dComIfG_gameInfo.play.particle;
    return !(result == &p.result && p.id == 0x85af && p.position == &position &&
        p.tev == &tev && p.rotation == &rotation && p.scale == &scale &&
        p.alpha == 173 && p.callback == &callback && p.room == -7 &&
        p.primary == &primary && p.environment == &environment &&
        p.particleScale == &particleScale && p.rate == 1.0f);
}
'''
with tempfile.TemporaryDirectory(prefix='albt-744-bindings-') as tmp:
    root = Path(tmp)
    (root / 'bridge.cpp').write_text(prefix + production)
    (root / 'caller.cpp').write_text(prefix + prototype + caller)
    executable = root / ('bindings.exe' if os.name == 'nt' else 'bindings')
    if os.name == 'nt':
        command = ['cl', '/nologo', '/std:c++20', '/EHsc', '/MD', '/I' + str(control), '/Fe:' + str(executable)]
    else:
        command = ['g++', '-std=c++20', '-D_WIN32=1', '-I' + str(control), '-o', str(executable)]
    subprocess.run(command + [str(root / 'bridge.cpp'), str(root / 'caller.cpp')], cwd=root, check=True)
    subprocess.run([str(executable)], check=True)
print('PASS: production ALBT full particle overload links with separate callers and preserves every argument; Windows CRT names remain unmodified')
