"""Exercise real SDK hook state across signed modules and verify all staged resources."""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('source', type=Path)
parser.add_argument('--cache', type=Path)
parser.add_argument('--shared-namespace-negative-control', action='store_true')
args = parser.parse_args()
source = args.source.resolve()
control = Path(__file__).resolve().parents[1]
cache = args.cache or source / '.uwp-mod-downloads-736'
lock = json.loads((control / 'BUNDLED_MODS-736.json').read_text())

module_source = r'''
#include <mods/service.hpp>
#include <mods/svc/hook.hpp>
DEFINE_MOD();
extern void tick(int&);
DEFINE_HOOK(&tick, SharedTick);
extern "C" void MODULE_SETUP(ModContext* context, const HookService* service) {
    mod_ctx = context;
    SharedTick::hooks = service;
    SharedTick::target = reinterpret_cast<void*>(&tick);
    SharedTick::g_orig = &tick;
}
extern "C" void MODULE_RUN(int* value) { SharedTick::trampoline(*value); }
extern "C" void MODULE_RESET() {
    SharedTick::hooks = nullptr;
    SharedTick::target = nullptr;
    SharedTick::g_orig = nullptr;
    mod_ctx = nullptr;
}
extern "C" const void* MODULE_RECORD() {
#if defined(__GNUC__) && !defined(__clang__) && defined(__ELF__)
    return &mod_meta_hook_SharedTick;
#else
    return &mods::detail::HookRecordFor<&tick, mods::FixedString{"&tick"}>::Holder::record;
#endif
}
'''

main_source = r'''
#include <mods/api.h>
#include <mods/svc/hook.h>
#include <cassert>
#include <iostream>
extern "C" {
void alpha_setup(ModContext*, const HookService*);
void beta_setup(ModContext*, const HookService*);
void alpha_run(int*); void beta_run(int*);
void alpha_reset(); void beta_reset();
const void* alpha_record(); const void* beta_record();
extern const ModMeta alpha_mod_meta;
extern const ModMeta beta_mod_meta;
}
void tick(int& value) { value += 7; }
int alphaContext, betaContext;
int alphaPre=0, betaPre=0, alphaPost=0, betaPost=0;
ModResult before(ModContext* ctx, void* target, void*, void*, int* skip) {
    assert(target == reinterpret_cast<void*>(&tick));
    if(ctx == reinterpret_cast<ModContext*>(&alphaContext)) ++alphaPre;
    else { assert(ctx == reinterpret_cast<ModContext*>(&betaContext)); ++betaPre; }
    *skip = 0; return MOD_OK;
}
ModResult after(ModContext* ctx, void* target, void*, void*) {
    assert(target == reinterpret_cast<void*>(&tick));
    if(ctx == reinterpret_cast<ModContext*>(&alphaContext)) ++alphaPost;
    else { assert(ctx == reinterpret_cast<ModContext*>(&betaContext)); ++betaPost; }
    return MOD_OK;
}
int main() {
    HookService service{}; service.dispatch_pre=&before; service.dispatch_post=&after;
    alpha_setup(reinterpret_cast<ModContext*>(&alphaContext), &service);
    beta_setup(reinterpret_cast<ModContext*>(&betaContext), &service);
    assert(alpha_record()!=beta_record());
#ifdef _WIN32
    assert(alpha_mod_meta.records_begin != beta_mod_meta.records_begin);
    const auto in_range=[](const ModMeta& meta, const void* record) {
        auto value=reinterpret_cast<uintptr_t>(record);
        return value>=reinterpret_cast<uintptr_t>(meta.records_begin) &&
               value<reinterpret_cast<uintptr_t>(meta.records_end);
    };
    assert(in_range(alpha_mod_meta, alpha_record()));
    assert(in_range(beta_mod_meta, beta_record()));
    assert(!in_range(alpha_mod_meta, beta_record()));
    assert(!in_range(beta_mod_meta, alpha_record()));
#endif
    int value=0;
    for(int i=0;i<1000;++i) { alpha_run(&value); beta_run(&value); }
    assert(value==14000 && alphaPre==1000 && betaPre==1000);
    assert(alphaPost==1000 && betaPost==1000);
    alpha_reset(); beta_run(&value);
    assert(value==14007 && betaPre==1001 && betaPost==1001);
    alpha_setup(reinterpret_cast<ModContext*>(&alphaContext), &service);
    alpha_run(&value); assert(alphaPre==1001 && alphaPost==1001 && value==14014);
    alpha_reset(); beta_reset();
    std::cout << "PASS .736 independent hook records, contexts, idle dispatch and reset/re-enable\n";
}
'''

policy_source = r'''
#include "dusk/mods/uwp_ports.hpp"
#include <cassert>
#include <string_view>
int main() {
    namespace ports=dusk::mods::uwp_ports;
    assert(ports::available("org.dusklight.tp_classic_buttons","1.3.2"));
    assert(ports::available("dev.twilitrealm.luau","1.0.0"));
    for (auto version : {"1.3.1", "1.3.3", "1.4.0", "", "1.3.2-unknown"})
        assert(!ports::available("org.dusklight.tp_classic_buttons",version));
    assert(!ports::available("dev.twilitrealm.randomizer","1.0.5"));
    assert(!ports::available("another.native.mod","1.3.2"));
    assert(ports::enabled_by_default("dev.twilitrealm.randomizer"));
    assert(ports::enabled_by_default("dev.twilitrealm.cosmetics"));
    assert(ports::enabled_by_default("dev.twilitrealm.luau"));
    assert(!ports::enabled_by_default("org.dusklight.tp_classic_buttons"));
    assert(!ports::enabled_by_default("com.ditrey.linkle"));
    assert(ports::enabled_by_default("existing.asset.mod", false));
    assert(!ports::enabled_by_default("existing.asset.mod", true));
    assert(!ports::enabled_by_default("org.dusklight.tp_classic_buttons", false));
}
'''

with tempfile.TemporaryDirectory(prefix='tpr-736-ports-') as temporary:
    work = Path(temporary)
    compiler = shutil.which('cl' if os.name == 'nt' else 'g++')
    if not compiler:
        raise RuntimeError('C++ compiler unavailable')
    objects = []
    for name, flag in [('alpha', 'DUSK_STATIC_CONTROLLER_UI'), ('beta', 'DUSK_STATIC_LUAU_RUNTIME')]:
        path = work / (name + '.cpp')
        path.write_text(module_source, encoding='utf-8')
        defines = [flag+'=1', 'DUSK_STATIC_BUILTIN_MOD=1', 'DUSK_MOD_FEATURE_GAME=1',
                   'mod_ctx='+name+'_mod_ctx', 'mod_meta='+name+'_mod_meta',
                   'mod_meta_bounds_begin='+name+'_mod_meta_bounds_begin',
                   'mod_meta_bounds_end='+name+'_mod_meta_bounds_end',
                   'MODULE_SETUP='+name+'_setup', 'MODULE_RUN='+name+'_run',
                   'MODULE_RESET='+name+'_reset', 'MODULE_RECORD='+name+'_record']
        if not args.shared_namespace_negative_control:
            defines.append('mods='+name+'_sdk')
        obj = work / (name + ('.obj' if os.name == 'nt' else '.o'))
        if os.name == 'nt':
            command = [compiler, '/nologo', '/std:c++20', '/EHsc', '/utf-8', '/c',
                       '/I'+str(source/'sdk/include'), '/Fo:'+str(obj), str(path)]
            command += ['/D'+value for value in defines]
        else:
            command = [compiler, '-std=c++20', '-O1', '-w', '-c', '-I'+str(source/'sdk/include'),
                       '-o', str(obj), str(path)] + ['-D'+value for value in defines]
        subprocess.run(command, check=True, cwd=work)
        objects.append(str(obj))
    main = work/'main.cpp'; main.write_text(main_source, encoding='utf-8')
    exe = work/('ports.exe' if os.name == 'nt' else 'ports')
    if os.name == 'nt':
        command = [compiler, '/nologo', '/std:c++20', '/EHsc', '/utf-8',
                   '/I'+str(source/'sdk/include'), str(main), *objects, '/Fe:'+str(exe)]
    else:
        command = [compiler, '-std=c++20', '-O1', '-I'+str(source/'sdk/include'),
                   str(main), *objects, '-o', str(exe)]
    subprocess.run(command, check=True, cwd=work)
    result = subprocess.run([str(exe)], cwd=work)
    if args.shared_namespace_negative_control:
        if result.returncode == 0:
            raise RuntimeError('Negative control unexpectedly kept shared hook contexts separate')
        print('PASS .736 negative control: shared SDK namespaces reproduce cross-mod hook state corruption')
        raise SystemExit(0)
    if result.returncode:
        raise RuntimeError('Signed module state isolation failed')
    policy = work/'policy.cpp'; policy.write_text(policy_source, encoding='utf-8')
    exe = work/('policy.exe' if os.name == 'nt' else 'policy')
    command = ([compiler, '/nologo', '/std:c++20', '/EHsc', '/utf-8',
                '/I'+str(source/'src'), str(policy), '/Fe:'+str(exe)] if os.name == 'nt' else
               [compiler, '-std=c++20', '-I'+str(source/'src'), str(policy), '-o', str(exe)])
    subprocess.run(command, check=True, cwd=work)
    subprocess.run([str(exe)], check=True, cwd=work)

count = 0
for mod in lock['mods']:
    if not mod['bundle']:
        continue
    root = (source/'mods/controller_ui' if mod['id']=='org.dusklight.tp_classic_buttons' else
            source/'platforms/uwp/optional_mods'/mod['id'])
    bundle = cache/(mod['id']+'_'+mod['version']+'.dusk')
    with zipfile.ZipFile(bundle) as archive:
        for entry in archive.infolist():
            if entry.is_dir() or entry.filename.lower().endswith(('.dll','.so','.dylib')):
                continue
            actual = root/entry.filename
            if not actual.is_file() or actual.read_bytes() != archive.read(entry):
                raise RuntimeError('Staged mod resource mismatch: '+mod['id']+'/'+entry.filename)
    manifest = json.loads((root/'mod.json').read_text())
    assert manifest['id']==mod['id'] and manifest['version']==mod['version']
    assert not any(path.suffix.lower() in ('.dll','.so','.dylib') for path in root.rglob('*'))
    count += 1
assert count==22
pin=lock['controllerSource']
assert hashlib.sha256((source/'mods/controller_ui/src/mod.cpp').read_bytes()).hexdigest()==pin['sha256']
assert 'XandasLegend' in (source/'mods/controller_ui/res/LICENSE.txt').read_text()
print('PASS .736 pinned native version policy, optional defaults, 22 exact catalog payloads and controller source/license')
