"""Exercise production SDK definitions for every additional signed native port."""
import argparse,ast,json,os,re,shutil,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('--shared-namespace-negative-control',action='store_true');a=p.parse_args();source=a.source.resolve();control=Path(__file__).resolve().parents[1]
mods=json.loads((control/'NATIVE_MODS-743.json').read_text())['mods']
legacy=ast.parse((control/'scripts/test-736-mod-ports.py').read_text());fixtures={}
for node in legacy.body:
    if isinstance(node,ast.Assign) and isinstance(node.targets[0],ast.Name) and node.targets[0].id in ['module_source','main_source']:
        fixtures[node.targets[0].id]=ast.literal_eval(node.value)
sdk=(source/'cmake/ModSDK.cmake').read_text();blocks=re.findall(r'target_compile_definitions\(\$\{target_name\} PRIVATE\s+([^)]*)\)',sdk)
common=next(b for b in blocks if b.strip()=='DUSK_STATIC_BUILTIN_MOD=1').split()
renames=next(b for b in blocks if 'mod_ctx=${target_name}_mod_ctx' in b).split()
def definitions(mod):
    target=mod['target'];cm=(source/'mods/uwp_ports'/target/'CMakeLists.txt').read_text()
    actual=next(b for b in re.findall(r'target_compile_definitions\('+re.escape(target)+r' PRIVATE\s+([^)]*)\)',cm) if mod['metadataDefine'] in b).split()
    assert mod['metadataDefine']+'=1' in actual
    # Version strings and production shader/platform definitions are irrelevant
    # to this shared-hook fixture. Import/export/context and section names are not.
    actual=[d for d in actual if not '"' in d]
    return [d.replace('${target_name}',target) for d in common+renames+actual]+['DUSK_MOD_FEATURE_GAME=1']
compiler=shutil.which('cl' if os.name=='nt' else 'g++');assert compiler
with tempfile.TemporaryDirectory(prefix='ports-743-') as temp:
    work=Path(temp)
    pairs=list(zip(mods,mods[1:]+mods[:1]))
    if a.shared_namespace_negative_control:pairs=pairs[:1]
    for first,second in pairs:
        objects=[];main=fixtures['main_source'].replace('PASS .736','PASS .743')
        if os.name=='nt':
            main='#include <cstdlib>\n'+main.replace('int main() {','int main() {\n    _set_error_mode(_OUT_TO_STDERR);\n    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);')
        for name,mod in zip(['alpha','beta'],[first,second]):
            f=work/(name+'.cpp');f.write_text(fixtures['module_source']);defs=definitions(mod)
            meta=next(d.split('=',1)[1] for d in defs if d.startswith('mod_meta='));main=main.replace(name+'_mod_meta',meta)
            defs+=['MODULE_SETUP='+name+'_setup','MODULE_RUN='+name+'_run','MODULE_RESET='+name+'_reset','MODULE_RECORD='+name+'_record']
            if a.shared_namespace_negative_control:defs=[d for d in defs if not d.startswith('mods=')]
            obj=work/(name+('.obj' if os.name=='nt' else '.o'))
            if os.name=='nt':cmd=[compiler,'/nologo','/std:c++20','/EHsc','/utf-8','/c','/I'+str(source/'sdk/include'),'/Fo:'+str(obj),str(f),*['/D'+d for d in defs]]
            else:cmd=[compiler,'-std=c++20','-O1','-w','-c','-I'+str(source/'sdk/include'),str(f),'-o',str(obj),*['-D'+d for d in defs]]
            subprocess.run(cmd,cwd=work,check=True,stdout=subprocess.DEVNULL);objects.append(str(obj))
        f=work/'main.cpp';f.write_text(main);exe=work/('ports.exe' if os.name=='nt' else 'ports')
        if os.name=='nt':cmd=[compiler,'/nologo','/std:c++20','/EHsc','/utf-8','/I'+str(source/'sdk/include'),str(f),*objects,'/Fe:'+str(exe)]
        else:cmd=[compiler,'-std=c++20','-O1','-w','-I'+str(source/'sdk/include'),str(f),*objects,'-o',str(exe)]
        subprocess.run(cmd,cwd=work,check=True,stdout=subprocess.DEVNULL);result=subprocess.run([str(exe)],cwd=work,stdout=subprocess.DEVNULL)
        if a.shared_namespace_negative_control:
            assert result.returncode!=0,'Removing production SDK isolation did not reproduce corruption'
            print('PASS: shared-namespace negative control reproduces cross-mod hook context corruption');break
        assert result.returncode==0,first['name']+' / '+second['name']+' hook context failure'
        print('PASS: independent production SDK contexts, hook records and reset/re-enable for '+first['name']+' / '+second['name'],flush=True)
    policy=work/'policy.cpp';lines=['#include "dusk/mods/uwp_ports.hpp"','#include <cassert>','int main(){namespace p=dusk::mods::uwp_ports;']
    for m in mods:
        lines+=['assert(p::available('+json.dumps(m['id'])+','+json.dumps(m['version'])+'));','assert(p::native_id_known('+json.dumps(m['id'])+'));','assert(!p::available('+json.dumps(m['id'])+',"0.0.0-unknown"));','assert(!p::enabled_by_default('+json.dumps(m['id'])+',true));']
    lines+=['assert(!p::available("unknown.native.mod","1.0.0"));','assert(p::enabled_by_default("dev.twilitrealm.randomizer"));','assert(p::enabled_by_default("dev.twilitrealm.cosmetics"));','assert(p::enabled_by_default("dev.twilitrealm.luau"));','}'];policy.write_text('\n'.join(lines));exe=work/('policy.exe' if os.name=='nt' else 'policy')
    if os.name=='nt':cmd=[compiler,'/nologo','/std:c++20','/EHsc','/I'+str(source/'src'),str(policy),'/Fe:'+str(exe)]
    else:cmd=[compiler,'-std=c++20','-I'+str(source/'src'),str(policy),'-o',str(exe)]
    subprocess.run(cmd,cwd=work,check=True,stdout=subprocess.DEVNULL);subprocess.run([str(exe)],check=True,cwd=work)
    print('PASS: every source-built native port has an exact version gate and disabled packaged default')
