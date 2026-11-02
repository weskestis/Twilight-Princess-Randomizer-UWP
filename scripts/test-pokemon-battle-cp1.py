"""Build and execute the real CP1 sources, optionally against Dusklight's SDK headers."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--sdk', type=Path, help='Path to the pinned Dusklight sdk directory')
    parser.add_argument('--sanitize', action='store_true', help='GCC/Clang address + UB sanitizers')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1] / 'experimental/pokemon_battle'
    if args.sdk and not (args.sdk / 'include/mods/svc/ui.h').is_file():
        parser.error('--sdk must contain include/mods/svc/ui.h')
    windows = os.name == 'nt'
    compiler = shutil.which('cl' if windows else 'g++')
    if not compiler:
        parser.error('MSVC cl or g++ must be available')
    if args.sanitize and windows:
        parser.error('--sanitize is supported by this runner on GCC/Clang only')
    with tempfile.TemporaryDirectory(prefix='pokemon-battle-cp1-') as directory:
        build = Path(directory)
        core = [root / 'src/battle.cpp', root / 'src/session.cpp']
        common = ([compiler, '/nologo', '/EHsc', '/std:c++20', '/W4', '/WX', '/utf-8',
                   '/I' + str(root / 'include')] if windows else
                  [compiler, '-std=c++20', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                   '-I' + str(root / 'include')])
        if args.sanitize:
            common += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g']

        def run(name, files, extra=None, arguments=None):
            executable = build / (name + ('.exe' if windows else ''))
            command = common + (extra or []) + [str(p) for p in files]
            command += (['/Fe:' + str(executable)] if windows else ['-o', str(executable)])
            subprocess.run(command, cwd=build, check=True)
            subprocess.run([str(executable)] + (arguments or []), cwd=build, check=True)

        run('battle_tests', core + [root / 'tests/battle_tests.cpp'])
        run('field_tests', [root / 'field/party.cpp', root / 'field/roster.cpp', root / 'field/ball.cpp', root / 'field/combat.cpp', root / 'tests/field_tests.cpp'])
        run('battle_demo', core + [root / 'demo/main.cpp'], arguments=['--autoplay'])
        if args.sdk:
            include = args.sdk.resolve() / 'include'
            # SDK metadata attributes are compiler-specific, and external to CP1.
            extra = (['/external:I' + str(include), '/external:W0'] if windows else
                     ['-isystem', str(include), '-Wno-attributes'])
            run('battle_ui_tests', [root / 'src/battle.cpp', root / 'native/mod.cpp',
                                    root / 'tests/ui_tests.cpp'], extra)
            run('party_save_tests', [root / 'field/party.cpp', root / 'field/roster.cpp', root / 'field/ball.cpp', root / 'tests/party_save_tests.cpp'], extra)
        else:
            print('SDK panel test skipped: pass --sdk to validate the adapter.')


if __name__ == '__main__':
    main()
