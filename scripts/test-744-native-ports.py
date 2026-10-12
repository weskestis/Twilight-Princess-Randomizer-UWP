"""Exercise actual SDK isolation, version gates and bounded native audio lifetime."""
import argparse
import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('source', type=Path)
a = p.parse_args()
source = a.source.resolve()
control = Path(__file__).resolve().parents[1]
# Reuse the production SDK hook/context fixture with the three new descriptors.
text = (control / 'scripts/test-743-native-ports.py').read_text()
text = text.replace("mods=json.loads((control/'NATIVE_MODS-743.json').read_text())['mods']", "mods=json.loads((control/'NATIVE_MODS-744.json').read_text())['mods']")
text = text.replace('PASS .743', 'PASS .744')
exec(compile(text, str(control / 'scripts/test-743-native-ports.py'), 'exec'), {'__file__': str(control / 'scripts/test-743-native-ports.py'), '__name__': '__main__'})
compiler = shutil.which('cl' if os.name == 'nt' else 'g++')
assert compiler
with tempfile.TemporaryDirectory(prefix='audio-744-') as temp:
    work = Path(temp)
    exe = work / ('audio.exe' if os.name == 'nt' else 'audio')
    if os.name == 'nt':
        cmd = [compiler, '/nologo', '/std:c++20', '/EHsc', '/utf-8', '/I' + str(control / 'tests/fake_xaudio'), '/I' + str(control / 'native_ports'), str(control / 'tests/xbox_voice_playback.cpp'), '/Fe:' + str(exe)]
    else:
        cmd = [compiler, '-std=c++20', '-g', '-fsanitize=address,undefined', '-I' + str(control / 'tests/fake_xaudio'), '-I' + str(control / 'native_ports'), str(control / 'tests/xbox_voice_playback.cpp'), '-o', str(exe)]
    subprocess.run(cmd, cwd=work, check=True, stdout=subprocess.DEVNULL)
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
    subprocess.run([str(exe)], check=True, cwd=work, env=env)
    print('PASS: real Xbox playback adapter retains queued PCM until consumption/destruction; bounded buffers/peers, retirement, reset and failed audio calls')
