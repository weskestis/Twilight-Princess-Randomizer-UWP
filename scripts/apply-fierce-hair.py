"""Add the isolated signed hair port after the complete Xbox .745 layer."""
from pathlib import Path
import hashlib,json,os,shutil

root=Path(os.environ['TPR_SRC'])
control=Path(os.environ['TPR_CONTROL'])
identity=json.loads((control/'APP_IDENTITY-746.json').read_text())
for name,expected in identity['sourceControls'].items():
    assert hashlib.sha256((control/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==expected,name
assert 'Version="1.4.1.745"' in (root/'platforms/uwp/Package.appxmanifest').read_text()

def edit(name,fn):
    p=root/name;s=p.read_text().replace('\r\n','\n');t=fn(s)
    assert t!=s,'No change: '+name
    p.write_text(t,encoding='utf8',newline='\n')

def replace(s,old,new):
    assert s.count(old)==1,'Changed source anchor: '+old[:100]
    return s.replace(old,new)

target='port_fierce_hair'
dest=root/'mods/uwp_ports'/target
assert not dest.exists()
shutil.copytree(control/'native_ports/fierce_hair',dest)
edit('mods/uwp_ports/CMakeLists.txt',lambda s:s+'\nadd_subdirectory(port_fierce_hair)\n')
edit('platforms/uwp/NativePorts743.cmake',lambda s:s+'\nlist(APPEND DUSK_UWP_ADDITIONAL_PORTS port_fierce_hair)\n')

def metadata(s):
    anchor='#elif defined(DUSK_STATIC_CONTROLLER_UI)'
    assert s.count(anchor)==2
    record='#elif defined(DUSK_STATIC_PORT_31)\n'+''.join('#pragma section(".duskP31$'+x+'", read, write)\n' for x in 'adz')+'#define DUSK_MOD_META_RECORD_SECTION ".duskP31$d"\n'
    bounds='#elif defined(DUSK_STATIC_PORT_31)\n#define DUSK_MOD_META_BOUNDS_A ".duskP31$a"\n#define DUSK_MOD_META_BOUNDS_Z ".duskP31$z"\n'
    s=s.replace(anchor,record+anchor,1)
    pos=s.index('#define DUSK_MOD_META_BOUNDS_A')
    return s[:pos]+replace(s[pos:],anchor,bounds+anchor)

edit('sdk/include/mods/meta.hpp',metadata)
edit('src/dusk/mods/uwp_ports.hpp',lambda s:replace(s,
    'inline constexpr AdditionalPortVersion AdditionalPorts[] = {',
    'inline constexpr AdditionalPortVersion AdditionalPorts[] = {\n    {"tpr.optional.fierce_deity_hair_physics", "0.1.0"},'))

def loader(s):
    declarations='\nextern ModContext* port_fierce_hair_mod_ctx;\nextern const ModMeta port_fierce_hair_mod_meta;\nModResult port_fierce_hair_mod_initialize(ModError*);\nModResult port_fierce_hair_mod_update(ModError*);\nModResult port_fierce_hair_mod_shutdown(ModError*);'
    anchor='ModResult luau_runtime_mod_shutdown(ModError*);'
    s=replace(s,anchor,anchor+declarations)
    anchor='    } else {\n        return false;\n    }\n    if (!parse_meta(*native, mod))'
    dispatch='    } else if (mod.metadata.id == "tpr.optional.fierce_deity_hair_physics") {\n        native->meta = &port_fierce_hair_mod_meta;\n        native->contextSymbol = &port_fierce_hair_mod_ctx;\n        native->fn_initialize = &port_fierce_hair_mod_initialize;\n        native->fn_update = &port_fierce_hair_mod_update;\n        native->fn_shutdown = &port_fierce_hair_mod_shutdown;\n'
    return replace(s,anchor,dispatch+anchor)

edit('src/dusk/mods/loader/natives.cpp',loader)

# Do not silently build on a runtime lacking member-level model replacement.
arc=(root/'libs/JSystem/src/JKernel/JKRArchivePri.cpp').read_text()
assert 'path.resize(path.size() - extension.size());' in arc
assert 'mArcOverlaysPath + pathIt->second' in arc
assert 'getOverlayData(fileEntry, resourceSize)' in (root/'libs/JSystem/src/JKernel/JKRMemArchive.cpp').read_text()

for name in ['src/dusk/startup_guard.hpp','src/dusk/ui/prelaunch.cpp',
             'mods/randomizer/src/session.cpp','platforms/uwp/Package.appxmanifest',
             'src/dusk/xbox_memory_budget.hpp']:
    def advance(s):
        assert '1.4.1.745' in s and '1.4.1.746' not in s
        s=s.replace('1.4.1.745','1.4.1.746')
        if name=='src/dusk/startup_guard.hpp':
            s=replace(s,'detail::markerPath = userPath / "xbox-startup-stage-745.txt";',
                      'detail::markerPath = userPath / "xbox-startup-stage-746.txt";')
            # Retain the .745 crash journal with the earlier journal history.
            old=json.loads((control/'APP_IDENTITY-745.json').read_text())['previousStartupJournals']
            before='{'+', '.join(json.dumps(v) for v in old)+'}'
            after='{'+', '.join(json.dumps(v) for v in ['xbox-startup-stage-745.txt']+old)+'}'
            s=replace(s,before,after)
        return s
    edit(name,advance)

(root/'platforms/uwp/optional_mods/FIERCE-HAIR-746.json').write_text(json.dumps(identity,indent=2)+'\n')
print('PASS: signed isolated hair port, exact version gate, native archive-member overlays and .746 identities')
