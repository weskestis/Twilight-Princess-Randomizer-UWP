"""Link Dawnlight's actual config bridge with a host, then exercise both build modes."""
import argparse
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('source', type=Path)
args = parser.parse_args()
source = args.source / 'mods/uwp_ports/port_dawnlight/src/survivors_integrity.cpp'
text = source.read_text()
# Compile the production bridge verbatim; the fixture supplies only its host ABI.
bridge = text[text.index('// These private host methods'):text.index('namespace dawnlight {')]
declarations = '''
namespace dusk::config {
class ConfigVarBase {
public:
    bool has_subscribers() const;
    void notify_changed(const void* previous);
};
}
'''
host = '''
int notifications = 0;
bool dusk::config::ConfigVarBase::has_subscribers() const { return true; }
void dusk::config::ConfigVarBase::notify_changed(const void*) { ++notifications; }
int main() {
    dusk::config::ConfigVarBase value;
    value.notify_changed(nullptr);
    return value.has_subscribers() && notifications == 1 ? 0 : 1;
}
'''
standalone = '''
int notifications = 0;
int main() {
    dusk::config::ConfigVarBase value;
    if (value.has_subscribers()) return 1;
    value.notify_changed(nullptr);
    s_nativeHasSubscribers = [](const dusk::config::ConfigVarBase*) { return true; };
    s_nativeNotifyChanged = [](dusk::config::ConfigVarBase*, const void*) { ++notifications; };
    value.notify_changed(nullptr);
    return value.has_subscribers() && notifications == 1 ? 0 : 2;
}
'''
compiler = shutil.which('cl' if os.name == 'nt' else 'g++')
assert compiler, 'C++ compiler is required for the host-link regression'
with tempfile.TemporaryDirectory(prefix='dawnlight-link-755-') as temporary:
    work = Path(temporary)
    (work / 'mod.cpp').write_text(declarations + bridge)
    (work / 'host.cpp').write_text(declarations + host)
    (work / 'standalone.cpp').write_text(declarations + bridge + standalone)

    def build(name, inputs, builtin=False):
        executable = work / (name + ('.exe' if os.name == 'nt' else ''))
        if os.name == 'nt':
            command = [compiler, '/nologo', '/std:c++20', '/EHsc', '/Fe:' + str(executable)]
            if builtin:
                command.append('/DDUSK_STATIC_BUILTIN_MOD=1')
        else:
            command = [compiler, '-std=c++20', '-o', str(executable)]
            if builtin:
                command.append('-DDUSK_STATIC_BUILTIN_MOD=1')
        result = subprocess.run(command + inputs, cwd=work, capture_output=True, text=True)
        return result, executable

    result, executable = build('builtin', ['mod.cpp', 'host.cpp'], builtin=True)
    assert result.returncode == 0, result.stdout + result.stderr
    subprocess.run([str(executable)], check=True)
    print('PASS: built-in Dawnlight links to the host config methods and preserves notifications')
    result, executable = build('standalone', ['standalone.cpp'])
    assert result.returncode == 0, result.stdout + result.stderr
    subprocess.run([str(executable)], check=True)
    print('PASS: standalone Dawnlight retains its resolved host bridge')
    result, _ = build('duplicate-control', ['mod.cpp', 'host.cpp'])
    diagnostic = result.stdout + result.stderr
    assert result.returncode != 0 and ('LNK2005' in diagnostic or 'multiple definition' in diagnostic), diagnostic
    assert 'has_subscribers' in diagnostic and 'notify_changed' in diagnostic, diagnostic
    print('PASS: negative control reproduces both duplicate host-symbol link failures')
