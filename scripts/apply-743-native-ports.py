"""Register separately scoped, signed source-built native ports in the UWP host."""
import json,os
from pathlib import Path
root=Path(os.environ['TPR_SRC']);control=Path(os.environ.get('TPR_CONTROL',Path(__file__).resolve().parents[1]));mods=json.loads((control/'NATIVE_MODS-743.json').read_text())['mods']
def edit(name,callback):
    path=root/name;text=path.read_text(encoding='utf-8').replace('\r\n','\n');after=callback(text)
    if after==text:raise RuntimeError('Native port edit changed nothing: '+name)
    path.write_text(after,encoding='utf-8',newline='\n')
def replace(text,old,new):
    if text.count(old)!=1:raise RuntimeError('Native source anchor changed: '+old[:100])
    return text.replace(old,new)
edit('CMakeLists.txt',lambda text:replace(text,'        add_subdirectory(mods/controller_ui)','        add_subdirectory(mods/controller_ui)\n        add_subdirectory(mods/uwp_ports)'))
def cmake(text):
    text=replace(text,'OR target_name STREQUAL "controller_ui" OR target_name STREQUAL "luau_runtime"))','OR target_name STREQUAL "controller_ui" OR target_name STREQUAL "luau_runtime"\n                OR target_name MATCHES "^port_"))')
    text=replace(text,'if (target_name STREQUAL "controller_ui" OR target_name STREQUAL "luau_runtime")','if (target_name STREQUAL "controller_ui" OR target_name STREQUAL "luau_runtime"\n                    OR target_name MATCHES "^port_")')
    text=replace(text,'                else ()\n                    target_compile_definitions(${target_name} PRIVATE DUSK_STATIC_LUAU_RUNTIME=1)','                elseif (target_name STREQUAL "luau_runtime")\n                    target_compile_definitions(${target_name} PRIVATE DUSK_STATIC_LUAU_RUNTIME=1)')
    return text
edit('cmake/ModSDK.cmake',cmake)
def meta(text):
    anchor='#if defined(DUSK_STATIC_CONTROLLER_UI)';assert text.count(anchor)==2
    sections=[];bounds=[]
    for i,mod in enumerate(mods):
        condition=('#if ' if i==0 else '#elif ')+'defined('+mod['metadataDefine']+')';section=mod['metadataSection']
        sections.append(condition+'\n'+''.join('#pragma section("'+section+'$'+letter+'", read, write)\n' for letter in ['a','d','z'])+'#define DUSK_MOD_META_RECORD_SECTION "'+section+'$d"\n')
        bounds.append(condition+'\n#define DUSK_MOD_META_BOUNDS_A "'+section+'$a"\n#define DUSK_MOD_META_BOUNDS_Z "'+section+'$z"\n')
    text=text.replace(anchor,''.join(sections)+'#elif defined(DUSK_STATIC_CONTROLLER_UI)',1)
    text=text.replace(anchor,''.join(bounds)+'#elif defined(DUSK_STATIC_CONTROLLER_UI)',1)
    return text
edit('sdk/include/mods/meta.hpp',meta)
def versions(text):
    anchor='constexpr bool available(std::string_view id, std::string_view version) noexcept {'
    declarations='struct AdditionalPortVersion { std::string_view id, version; };\ninline constexpr AdditionalPortVersion AdditionalPorts[] = {\n'+''.join('    {"'+m['id']+'", "'+m['version']+'"},\n' for m in mods)+'};\nconstexpr bool native_id_known(std::string_view id) noexcept {\n    for (const auto& port : AdditionalPorts) { if (port.id == id) return true; }\n    return false;\n}\n\n'
    text=replace(text,anchor,declarations+anchor+'\n    for (const auto& port : AdditionalPorts) {\n        if (port.id == id && port.version == version) return true;\n    }')
    return text
edit('src/dusk/mods/uwp_ports.hpp',versions)
def natives(text):
    anchor='ModResult luau_runtime_mod_shutdown(ModError*);'
    declarations='\n'.join('extern ModContext* '+m['target']+'_mod_ctx;\nextern const ModMeta '+m['target']+'_mod_meta;\nModResult '+m['target']+'_mod_initialize(ModError*);\nModResult '+m['target']+'_mod_update(ModError*);\nModResult '+m['target']+'_mod_shutdown(ModError*);' for m in mods)
    text=replace(text,anchor,anchor+'\n'+declarations)
    text=replace(text,'    } else {\n        native->meta = &luau_runtime_mod_meta;','    } else if (mod.metadata.id == uwp_ports::LuauId) {\n        native->meta = &luau_runtime_mod_meta;')
    anchor='        native->fn_shutdown = &luau_runtime_mod_shutdown;\n    }\n    if (!parse_meta(*native, mod))'
    dispatch=''
    for m in mods:
        t=m['target'];dispatch+='    } else if (mod.metadata.id == "'+m['id']+'") {\n        native->meta = &'+t+'_mod_meta;\n        native->contextSymbol = &'+t+'_mod_ctx;\n        native->fn_initialize = &'+t+'_mod_initialize;\n        native->fn_update = &'+t+'_mod_update;\n        native->fn_shutdown = &'+t+'_mod_shutdown;\n'
    text=replace(text,anchor,'        native->fn_shutdown = &luau_runtime_mod_shutdown;\n'+dispatch+'    } else {\n        return false;\n    }\n    if (!parse_meta(*native, mod))')
    text=replace(text,'if (mod.metadata.id == uwp_ports::ControllerId || mod.metadata.id == uwp_ports::LuauId) {','if (mod.metadata.id == uwp_ports::ControllerId || mod.metadata.id == uwp_ports::LuauId ||\n        uwp_ports::native_id_known(mod.metadata.id)) {')
    return text
edit('src/dusk/mods/loader/natives.cpp',natives)
def platform(text):
    text=replace(text,'foreach (_port controller_ui luau_runtime)','include("${CMAKE_CURRENT_LIST_DIR}/NativePorts743.cmake")\nforeach (_port controller_ui luau_runtime ${DUSK_UWP_ADDITIONAL_PORTS})')
    anchor='    set(_port_archive "${DUSK_DIR}/mods/${_port}/${_port}.lib")'
    text=replace(text,anchor,'''    if (_port MATCHES "^port_")
        set(_port_archive "${DUSK_DIR}/mods/uwp_ports/${_port}/${_port}.lib")
    else ()
'''+anchor+'''\n    endif ()''')
    return text
edit('platforms/uwp/CMakeLists.txt',platform)
print('Registered '+str(len(mods))+' signed native ports with individual SDK contexts, imports, metadata sections and version gates')
