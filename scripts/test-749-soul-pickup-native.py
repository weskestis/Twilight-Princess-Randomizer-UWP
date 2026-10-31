"""Exercise full production procCoGetItem with late actors and without its detour."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile


def function(source, signature):
    start = source.index(signature)
    tokens = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/|[{}]')
    depth = 0
    for match in tokens.finditer(source, start):
        if match.group() == '{':
            depth += 1
        elif match.group() == '}':
            depth -= 1
            if depth == 0:
                return source[start:match.end()] + '\n'
    raise RuntimeError('Unclosed production function: ' + signature)


p = argparse.ArgumentParser()
p.add_argument('source', type=Path)
p.add_argument('--negative-control', action='store_true')
a = p.parse_args()
r = a.source.resolve()
c = Path(__file__).resolve().parents[1]
mod = r / 'mods/randomizer/src'
hooks = (mod / 'hooks.cpp').read_text()
messages = (mod / 'messages.cpp').read_text()
tools = (mod / 'tools.cpp').read_text()
native = (r / 'src/d/actor/d_a_alink_demo.inc').read_text()
context = (mod / 'randomizer_context.cpp').read_text()
fixture = (c / 'tests/soul_pickup_native_749.cpp').read_text()
parts = {
    'MESSAGE_CONSTANTS': '\n'.join(line for line in (mod / 'custom_flow_ids.hpp').read_text().splitlines() if line.startswith('inline constexpr')),
    'PRODUCTION_MESSAGE_RESOLVER': function(messages, 'std::uint32_t pickup_message_id('),
    'PRODUCTION_BOSS_SOUL_PREDICATE': context[context.index('struct BossSoulGate {'):context.index('const BossSoulGate* FindBossSoulGate(')] + function(context, 'bool randomizer_IsBossSoulItem('),
    'PRODUCTION_PHYSICAL_ITEM': function((mod / 'enemy_souls.cpp').read_text(), 'std::uint8_t physical_item('),
    'PRODUCTION_ITEM_MESSAGE_FUNCTIONS': function(tools, 'u16 getItemMessageID(') + function(tools, 'u16 getSoulItemMessageID('),
    'PRODUCTION_PICKUP_HOOK': function(hooks, 'HookAction hookPreProcCoGetItem('),
    'PRODUCTION_NATIVE_BRIDGES': function(hooks, 'extern "C" u32 tpr_resolve_soul_pickup_message('),
    'PRODUCTION_NATIVE_PICKUP': function(native, 'BOOL daAlink_c::checkEndMessage(') + function(native, 'int daAlink_c::procCoGetItem()'),
    'PRODUCTION_RECOVERY': function(hooks, 'void recoverMissingPickupMessage(') + function(hooks, 'extern "C" BOOL tpr_complete_breakable_pickup_message(').replace('PickupMessageGuard::Clock::now()', 'fixtureNow'),
    'EXTRA_NATIVE_BRIDGE_USING': '',
}
extra = 'extern "C" u32 tpr_resolve_soul_pickup_for_actor('
if extra in hooks:
    parts['PRODUCTION_MESSAGE_RESOLVER'] += function(messages, 'bool soul_pickup_messages_active(')
    parts['PRODUCTION_NATIVE_BRIDGES'] += function(hooks, extra)
    parts['EXTRA_NATIVE_BRIDGE_USING'] = 'using randomizer::tpr_resolve_soul_pickup_for_actor;'
for marker, body in parts.items():
    fixture = fixture.replace('/* ' + marker + ' */', body)
with tempfile.TemporaryDirectory(prefix='soul-native-749-') as temp:
    w = Path(temp)
    src = w / 'test.cpp'
    src.write_text(fixture, encoding='utf-8')
    exe = w / ('test.exe' if os.name == 'nt' else 'test')
    includes = [r / 'sdk/include', mod, r / 'include']
    command = ['cl', '/nologo', '/EHsc', '/std:c++20', '/O2', '/D_UWP=1', '/DTARGET_PC=1', *['/I' + str(i) for i in includes], str(src), '/Fe:' + str(exe)] if os.name == 'nt' else ['c++', '-std=c++20', '-O2', '-D_UWP=1', '-DTARGET_PC=1', *['-I' + str(i) for i in includes], str(src), '-o', str(exe)]
    subprocess.run(command, cwd=w, check=True)
    result = subprocess.run([str(exe)], cwd=w)
    if a.negative_control:
        assert result.returncode == 42, 'Expected .748 to reproduce missed native Soul routing'
        print('PASS: .748 negative control exposes Soul routing outside the pre-hook')
    else:
        result.check_returncode()
