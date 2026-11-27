"""Integrate the world renderer and persistent video controls in the staged Xbox tree."""
import argparse
from pathlib import Path
import shutil

CONTROL = Path(__file__).resolve().parents[1]
OPTIONS = (
    ('worldBounce', 'Indirect Color Bounce', 65, 'Gather nearby surface colors and indirect sky light.'),
    ('worldLamps', 'Lantern and Local Lights', 75, 'Light nearby surfaces from the lantern and active stage and effect lights.'),
    ('worldShadows', 'Contact Shadows', 45, 'Use scene depth for short shadows from the sun and local lights.'),
    ('worldReflections', 'Water and Rain Reflections', 55, 'Trace visible scenery across nearby water and rain-wet upward facing surfaces.'),
    ('worldFog', 'Atmospheric Fog', 20, 'Add distance haze using the current room fog color and range.'),
    ('worldShafts', 'Sun Shafts', 45, 'Scatter light toward the actual sun through visible sky openings.'),
    ('worldFilmic', 'Filmic Tone Mapping', 20, 'Blend a filmic highlight curve into the world image.'),
)

def insert(root, path, anchor, block):
    file = root / path
    src = file.read_text(encoding='utf-8')
    if src.count(anchor) != 1:
        raise ValueError(f'Unexpected number of integration anchors in {path}: {src.count(anchor)}')
    file.write_text(src.replace(anchor, block + anchor), encoding='utf-8', newline='\n')

def stage(root):
    shader_dir = CONTROL / 'ports/lighting-756'
    target = root / 'src/dusk'
    shutil.copyfile(shader_dir / 'world_lighting.inc', target / 'world_lighting.inc')
    shader = (shader_dir / 'world_lighting.wgsl').read_text(encoding='utf-8')
    (target / 'world_lighting_shader.inc').write_text(
        'R"WORLD_LIGHTING(\n' + shader + ')WORLD_LIGHTING"\n', encoding='utf-8', newline='\n')
    insert(root, 'src/m_Do/m_Do_graphic.cpp', 'class mDoGph_HIO_c :',
           '#if defined(_UWP)\n#include "dusk/world_lighting.inc"\n#endif\n\n')
    # The translucent world has drawn, including water. HUD and pause overlays draw later.
    anchor = '            j3dSys.reinitGX();\n            GXSetClipMode(GX_CLIP_ENABLE);\n\n#if DEBUG\n            if (dJcame_c::get())'
    insert(root, 'src/m_Do/m_Do_graphic.cpp', anchor,
           '#if defined(_UWP)\n            dusk::world_lighting::record(camera_p->view);\n#endif\n')
    fields = [('worldLighting', 'bool', 'false'), ('worldLightingHeight', 'int', '540')]
    fields += [(key, 'int', str(default)) for key, _, default, _ in OPTIONS]
    insert(root, 'src/dusk/settings.h', '        ConfigVar<float> bloomMultiplier;',
           ''.join(f'        ConfigVar<{kind}> {key};\n' for key, kind, _ in fields))
    insert(root, 'src/dusk/settings.cpp', '        .bloomMultiplier {',
           ''.join(f'        .{key} {{"game.{key}", {default}}},\n' for key, _, default in fields))
    insert(root, 'src/dusk/settings.cpp', '    Register(g_userSettings.game.bloomMultiplier);',
           ''.join(f'    Register(g_userSettings.game.{key});\n' for key, _, _ in fields))
    section = '''        leftPane.add_section("World Lighting");
        config_bool_select(leftPane, rightPane, getSettings().game.worldLighting,
            {.key="Enable World Lighting", .helpText="Applies lighting to the world in both game modes."});
        auto lightingOff=[] { return !getSettings().game.worldLighting.getValue(); };
        config_int_select(leftPane, rightPane, getSettings().game.worldLightingHeight,
            "Lighting Resolution", "Maximum lighting pass height. Higher settings also trace more samples.",
            180, 1080, 180, lightingOff, {}, " px");
'''
    for key, title, _, desc in OPTIONS:
        section += f'        config_int_select(leftPane, rightPane, getSettings().game.{key},\n'
        section += f'            "{title}", "{desc}", 0, 100, 5, lightingOff, {{}}, "%");\n'
    insert(root, 'src/dusk/ui/settings.cpp', '        leftPane.add_section("Post-Processing");', section+'\n')

def version(root):
    for path in ('src/dusk/ui/prelaunch.cpp', 'mods/randomizer/src/session.cpp',
                 'platforms/uwp/Package.appxmanifest', 'src/dusk/xbox_memory_budget.hpp'):
        file = root / path
        data = file.read_text()
        if '1.4.1.755' not in data:
            raise ValueError('Expected .755 base in ' + path)
        file.write_text(data.replace('1.4.1.755', '1.4.1.756'), encoding='utf-8', newline='\n')
    path = root / 'src/dusk/startup_guard.hpp'
    data = path.read_text()
    anchor = 'detail::markerPath = userPath / "xbox-startup-stage-755.txt";'
    if data.count(anchor) != 1:
        raise ValueError('Unexpected startup journal identity')
    path.write_text(data.replace(anchor,anchor.replace('755','756')).replace(
        '{"xbox-startup-stage-754.txt",','{"xbox-startup-stage-755.txt", "xbox-startup-stage-754.txt",'),
        encoding='utf-8', newline='\n')

if __name__ == '__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('source',type=Path)
    args=parser.parse_args()
    stage(args.source)
    version(args.source)
