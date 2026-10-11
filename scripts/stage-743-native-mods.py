"""Stage pinned native sources and their matching resource-only release bundles."""
import argparse,hashlib,json,re,shutil,stat,subprocess,tempfile,urllib.request,zipfile
from pathlib import Path,PurePosixPath
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('--cache',type=Path);a=p.parse_args();root=a.source.resolve();control=Path(__file__).resolve().parents[1];lock=json.loads((control/'NATIVE_MODS-743.json').read_text());cache=(a.cache or root/'.uwp-native-downloads-743').resolve();cache.mkdir(parents=True,exist_ok=True)
def verified(entry):
    target=cache/entry['name']
    if not target.exists() or target.stat().st_size!=entry['size'] or hashlib.sha256(target.read_bytes()).hexdigest()!=entry['sha256']:
        request=urllib.request.Request(entry['url'],headers={'User-Agent':'Dusklight-UWP-Bundler/1.4.1.743','X-Dusklight-Version':'2.0.0-alpha2'})
        with urllib.request.urlopen(request,timeout=90) as response:data=response.read()
        if len(data)!=entry['size'] or hashlib.sha256(data).hexdigest()!=entry['sha256']:raise RuntimeError('Pinned download mismatch: '+entry['name'])
        target.write_bytes(data)
    return target
def safe(name):
    n=PurePosixPath(name)
    return not n.is_absolute() and '..' not in n.parts and '\\' not in name and ':' not in name
parent=root/'mods/uwp_ports';parent.mkdir(parents=True,exist_ok=True);targets=[];proof=[]
for mod in lock['mods']:
    target=mod['target'];directory=parent/target;directory.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(verified(mod['sourceArchive'])) as package:
        top=package.namelist()[0].split('/')[0]+'/'
        for name in mod['sourceFiles']:
            if not safe(name):raise RuntimeError('Unsafe source path')
            info=package.getinfo(top+name)
            if stat.S_ISLNK(info.external_attr>>16) or info.file_size>128*1024**2:raise RuntimeError('Unsafe source entry')
            f=directory/name;f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes(package.read(info))
    for spec in mod.get('sourcePatches',[]):
        patch_name=spec['name']
        if not safe(patch_name) or '/' in patch_name:raise RuntimeError('Unsafe native patch name')
        payload=(control/'patches'/patch_name).read_bytes().replace(b'\r\n',b'\n')
        if hashlib.sha256(payload).hexdigest()!=spec['sha256']:raise RuntimeError('Native source patch hash mismatch: '+patch_name)
        for line in payload.decode('utf-8').splitlines():
            if not line.startswith('+++ b/'):continue
            name=line[6:]
            if not safe(name) or name not in mod['sourceFiles']:raise RuntimeError('Native patch changes an unpinned source path')
            file=directory/name;file.write_bytes(file.read_bytes().replace(b'\r\n',b'\n'))
        with tempfile.TemporaryDirectory(prefix='native-patch-743-') as temp:
            patch=Path(temp)/patch_name;patch.write_bytes(payload)
            args=['git','-C',str(root),'apply','--directory=mods/uwp_ports/'+target]
            subprocess.run([*args,'--check',str(patch)],check=True)
            subprocess.run([*args,str(patch)],check=True)
    if mod.get('resourceSource')=='sourceArchive':
        with zipfile.ZipFile(verified(mod['sourceArchive'])) as package:
            top=package.namelist()[0].split('/')[0]+'/'
            for name in ['mod.json',*mod['sourceResourceFiles']]:
                if not safe(name):raise RuntimeError('Unsafe source resource path')
                f=directory/name;f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes(package.read(top+name))
        release=None
    else:
        release=verified({**mod['download'],'name':mod['id']+'.dusk'})
    if release is not None:
      with zipfile.ZipFile(release) as package:
        manifest=json.loads(package.read('mod.json'))
        if manifest['id']!=mod['id'] or manifest['version']!=mod['version']:raise RuntimeError('Native resource identity mismatch')
        names=set()
        for info in package.infolist():
            name=info.filename
            if not safe(name) or stat.S_ISLNK(info.external_attr>>16):raise RuntimeError('Unsafe native resource path')
            if name.casefold() in names:raise RuntimeError('Duplicate native resource path')
            names.add(name.casefold())
            if info.is_dir() or name.startswith('lib/') or name.lower().endswith(('.dll','.exe','.so','.dylib','.pdb')):continue
            if name!='mod.json' and not name.startswith(('res/','overlay/','textures/')):raise RuntimeError('Unexpected native bundle resource: '+name)
            f=directory/name;f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes(package.read(info))
    manifest=json.loads((directory/'mod.json').read_text())
    if manifest['id']!=mod['id'] or manifest['version']!=mod['version']:raise RuntimeError('Native source/resource identity mismatch')
    for license in directory.glob('*'):
        if license.is_file() and license.name.lower().startswith(('license','copying')):
            licenses=directory/'res/licenses';licenses.mkdir(parents=True,exist_ok=True);shutil.copyfile(license,licenses/license.name)
    cm='cmake_minimum_required(VERSION 3.25)\n# Source and matching catalog resources are hash-pinned by NATIVE_MODS-743.json.\n'
    cm+='add_mod('+target+'\n    FEATURES '+' '.join(mod['features'])+'\n    SOURCES '+' '.join(mod['sources'])+'\n    MOD_JSON mod.json\n'
    for name,key in [('res','RES_DIR'),('overlay','OVERLAY_DIR'),('textures','TEXTURES_DIR')]:
        if (directory/name).is_dir():cm+='    '+key+' '+name+'\n'
    cm+='    BUNDLE\n)\n'
    cm+='target_compile_definitions('+target+' PRIVATE '+mod['metadataDefine']+'=1\n'
    for variable in mod['serviceVariables']:cm+='    '+variable+'='+target+'_'+variable+'\n'
    for namespace in mod['privateNamespaces']:cm+='    '+namespace+'='+target+'_'+namespace+'\n'
    for symbol in mod.get('privateSymbols',[]):cm+='    '+symbol+'='+target+'_'+symbol+'\n'
    for define in mod.get('cmakeDefines',[]):cm+='    '+define+'\n'
    cm+=')\n'
    cm+='target_include_directories('+target+' PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/include")\n'
    for include in mod.get('includePaths',[]):
        path=include if include.startswith('${') else '${CMAKE_CURRENT_SOURCE_DIR}/'+include
        cm+='target_include_directories('+target+' PRIVATE "'+path+'")\n'
    if mod.get('linkTargets'):cm+='target_link_libraries('+target+' PRIVATE '+' '.join(mod['linkTargets'])+')\n'
    if target=='port_twilit_essentials':
        f=directory/'src/mod.cpp';text=f.read_text(encoding='utf-8-sig');text=text.replace('#ifdef _WIN32\nextern "C" __declspec(dllimport) void* __stdcall ShellExecuteA','#if defined(_WIN32) && !defined(_UWP)\nextern "C" __declspec(dllimport) void* __stdcall ShellExecuteA')
        old='''#ifdef _WIN32
    ShellExecuteA(nullptr, "open", kDiscordChannelUrl, nullptr, nullptr, 1 );'''
        new='''#if defined(_UWP)
    SDL_OpenURL(kDiscordChannelUrl);
#elif defined(_WIN32)
    ShellExecuteA(nullptr, "open", kDiscordChannelUrl, nullptr, nullptr, 1 );'''
        if text.count(old)!=1:raise RuntimeError('Essentials desktop shell anchor changed')
        f.write_text('#if defined(_UWP)\n#include <SDL3/SDL_misc.h>\n#endif\n'+text.replace(old,new),encoding='utf-8',newline='\n')
    if (directory/'src/CMakeLists.txt').is_file():cm+='add_subdirectory(src)\n'
    if 'English Names'==mod['name']:cm+='target_link_libraries('+target+' PRIVATE nlohmann_json::nlohmann_json)\n'
    # Preserve helper CMake references to the author's original target.
    for file in directory.rglob('CMakeLists.txt'):
        if file==directory/'CMakeLists.txt':continue
        text=file.read_text(encoding='utf-8-sig');text=re.sub(r'\b'+re.escape(mod['originalTarget'])+r'\b',target,text);file.write_text(text)
    (directory/'CMakeLists.txt').write_text(cm,encoding='utf-8',newline='\n');targets.append(target)
    proof.append({**mod,'resourceFiles':[{'path':str(f.relative_to(directory)).replace('\\','/'),'sha256':hashlib.sha256(f.read_bytes()).hexdigest(),'size':f.stat().st_size} for f in sorted(directory.rglob('*')) if f.is_file() and (f.relative_to(directory).parts[0] in ['res','overlay','textures'] or f.name=='mod.json')]})
    print('Staged '+mod['name']+' '+mod['version']+' for a signed static UWP build',flush=True)
(parent/'CMakeLists.txt').write_text('\n'.join('add_subdirectory('+t+')' for t in targets)+'\n')
(root/'platforms/uwp/NativePorts743.cmake').write_text('set(DUSK_UWP_ADDITIONAL_PORTS '+ ' '.join(targets)+')\n')
(root/'platforms/uwp/optional_mods/NATIVE-MODS-743.json').write_text(json.dumps({'mods':proof,'blocked':lock['blocked']},indent=2)+'\n')
print('PASS: '+str(len(targets))+' pinned native source ports staged; no downloaded native code retained')
