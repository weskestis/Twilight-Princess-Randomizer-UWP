"""Run production cosmetics/menu getters and UI callbacks against real ConfigVar layers."""
import argparse, os, subprocess, tempfile
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('source', type=Path)
p.add_argument('--negative-control', action='store_true')
a = p.parse_args()
r = a.source.resolve()
c = Path(__file__).resolve().parents[1]

def function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 1
    pos = brace + 1
    while depth:
        depth += (text[pos] == '{') - (text[pos] == '}')
        pos += 1
    return text[start:pos] + '\n'

mod = (r / 'mods/cosmetics/src/mod.cpp').read_text()
host = (r / 'src/dusk/ui/menu_colors.cpp').read_text()
runtime = r / 'mods/cosmetics/src/appearance_runtime.hpp'
if not a.negative_control:
    assert 'cosmetics.vanillaAppearance' in (r / 'src/dusk/settings.cpp').read_text()
    assert 'Register(g_userSettings.cosmetics.vanillaAppearance);' in (r / 'src/dusk/settings.cpp').read_text()
    assert 'ConfigVar<bool> vanillaAppearance;' in (r / 'src/dusk/settings.h').read_text()
    assert host.count('settings.vanillaAppearance.setValue(false);') == 3
    assert 'getSettings().cosmetics.vanillaAppearance.getValue() ||' in function(host, 'void randomize_menu_colors_for_load_screen()')

types = host[host.index('enum class MenuRole'):host.index('struct Rgb')]
types += function(host, 'struct RuntimeState').rstrip() + ';\n'
menu_signatures = ['std::array<ConfigVar<std::string>*, kRoleCount> color_vars()',
    'MenuPalette palette_at(', 'MenuPalette default_palette()', 'std::uint8_t selected_seed_mask()',
    'void set_runtime_color(', 'bool clear_runtime_color(', 'bool clear_runtime_colors()',
    'void apply_runtime_palette(', 'bool is_rainbow(', 'bool saved_menu_rainbow()', 'void update_menu_colors_impl(']
menu = ''.join(function(host, signature) for signature in menu_signatures)
modes = function(host, 'extern "C" unsigned tpr_appearance_mode()') + function(host, 'extern "C" void tpr_appearance_set_mode(')
ui_start = mod.index('void get_appearance_mode_control(')
new_prefix = 'constexpr std::array<unsigned, 5> kAppearanceUiModes'
if new_prefix in mod:
    ui_start = mod.index(new_prefix)
ui = mod[ui_start:mod.index('#endif', ui_start)]
getters = function(mod, 'std::string get_str_option(') + function(mod, 'int64_t get_int_option(')
manual = ''
if 'void get_manual_color_control(' in mod:
    getters = function(mod, 'std::string get_saved_str_option(') + getters
    manual = function((r / 'mods/cosmetics/src/color_utils.cpp').read_text(), 'bool is_valid_hex_color_str(')
    for signature in ['void get_manual_color_control(', 'void set_manual_color_control(',
        'bool is_manual_color_modified(', 'void get_manual_midna_control(', 'void set_manual_midna_control(',
        'bool is_manual_midna_modified(']:
        manual += function(mod, signature)
manual += 'template <size_t N>\n' + function(mod, 'void add_cosmetic_option(')
manual += function(mod, 'void add_midna_hair_option(')
fixture = (c / 'tests/cosmetics_off_748.cpp').read_text()
for marker, body in {'ACTUAL_MENU_TYPES': types, 'ACTUAL_MENU_FUNCTIONS': menu,
    'ACTUAL_HOST_MODES': modes, 'ACTUAL_UI_CALLBACKS': ui, 'ACTUAL_MOD_GETTERS': getters,
    'ACTUAL_MANUAL_COLOR_CALLBACKS': manual}.items():
    fixture = fixture.replace('/* ' + marker + ' */', body)
with tempfile.TemporaryDirectory(prefix='cosmetics-off-748-') as temp:
    w = Path(temp)
    source = w / 'production.cpp'
    source.write_text(fixture, encoding='utf-8')
    exe = w / ('test.exe' if os.name == 'nt' else 'test')
    includes = [r / 'mods/cosmetics/src', r / 'src', r / 'libs/dolphin/include']
    if os.name == 'nt':
        command = ['cl', '/nologo', '/EHsc', '/std:c++20', '/D_UWP=1', '/DTARGET_PC=1'] + ['/I' + str(v) for v in includes] + [str(source), '/Fe:' + str(exe)]
    else:
        command = ['c++', '-std=c++20', '-O1', '-D_UWP=1', '-DTARGET_PC=1'] + ['-I' + str(v) for v in includes] + [str(source), '-o', str(exe)]
    subprocess.run(command, cwd=w, check=True)
    result = subprocess.run([str(exe)], cwd=w)
    if a.negative_control:
        assert result.returncode != 0, 'Shipped .747 unexpectedly passes Vanilla/Off regression'
        print('PASS: shipped .747 reproduces the missing Vanilla/Off behavior')
    else:
        result.check_returncode()
