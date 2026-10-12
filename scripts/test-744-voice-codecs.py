"""Exercise both production Xbox voice paths with real Opus packets and replay/reset/loss."""
import argparse
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('source', type=Path)
p.add_argument('--opus', required=True, type=Path)
p.add_argument('--opus-include', required=True, type=Path)
p.add_argument('--sdl-include', required=True, type=Path)
a = p.parse_args()
control = Path(__file__).resolve().parents[1]
compiler = shutil.which('cl' if os.name == 'nt' else 'g++')
assert compiler and a.opus.is_file()
ports = a.source.resolve() / 'mods/uwp_ports'
for target, fixture, implementation, defines, includes in [
    ('port_crests_of_courage', 'crests_xbox_voice.cpp', 'src/voice_io.cpp', ['COOP_VOICE=1'], ['src']),
    ('port_mfb_multiplayer', 'mfb_xbox_voice.cpp', 'src/game/voice_chat.cpp', ['DUSKLIGHT_ONLINE_VOICE=1'], ['include', 'src/game']),
]:
    module = ports / target
    paths = [control / 'tests/fake_xaudio', a.opus_include.resolve(), a.sdl_include.resolve(), *[module / path for path in includes]]
    definitions = ['_UWP=1', *defines]
    with tempfile.TemporaryDirectory(prefix='voice-codec-744-') as temp:
        work = Path(temp)
        executable = work / ('voice.exe' if os.name == 'nt' else 'voice')
        inputs = [str(control / 'tests' / fixture), str(module / implementation)]
        if os.name == 'nt':
            command = [compiler, '/nologo', '/std:c++20', '/EHsc', '/MD', '/utf-8', *['/I' + str(path) for path in paths], *['/D' + d for d in definitions], *inputs, str(a.opus.resolve()), '/Fe:' + str(executable)]
        else:
            command = [compiler, '-std=c++20', '-g', *['-I' + str(path) for path in paths], *['-D' + d for d in definitions], *inputs, str(a.opus.resolve()), '-o', str(executable)]
        subprocess.run(command, check=True, cwd=work, stdout=subprocess.DEVNULL)
        subprocess.run([str(executable)], check=True, cwd=work)
        print('PASS: ' + target + ' real Opus decode, bounded native playback, replay rejection, packet loss, peer departure/rejoin and teardown')
