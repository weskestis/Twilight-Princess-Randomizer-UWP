"""Verify the restored source, then advance only the release identities."""
import hashlib,json,os
from pathlib import Path
r=Path(os.environ['TPR_SRC']);c=Path(os.environ['TPR_CONTROL'])
previous=json.loads((c/'APP_IDENTITY-746.json').read_text());identity=json.loads((c/'APP_IDENTITY-747.json').read_text())
assert previous['version']=='1.4.1.746' and identity['version']=='1.4.1.747'
for name,sha in identity['sourceControls'].items():
 if hashlib.sha256((c/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()!=sha:raise RuntimeError('.747 source control hash mismatch: '+name)
for name,sha in identity['sourcePatches'].items():
 if hashlib.sha256((c/'patches'/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()!=sha:raise RuntimeError('.747 patch hash mismatch: '+name)
required={
 'src/dusk/companion/server.cpp':['/v1/pair','/v1/snapshot','mSessions.empty()','now + 500ms','native_clients_only','pairing_required'],
 'src/dusk/ui/companion_window.cpp':['Pair Android','Approve matching code','dusklight://pair?api=1','image('],
 'src/dusk/ui/ui.cpp':['companion::update();','register_companion_texture();'],
 'mods/randomizer/src/session.cpp':['tpr_companion_snapshot','GenerateTrackerWorld(false)','randomizer_GetDiscoveredEntranceLog()','smallKeysFound'],
 'platforms/uwp/main.cpp':['BCryptGenRandom','INetworkInformationStatics','GetHostNames'],
 'platforms/uwp/Package.appxmanifest':['privateNetworkClientServer','internetClientServer'],
 'src/dusk/ui/mods_window.cpp':['std::chrono::milliseconds(180)','commit_detail(true);','if (mCulled) return;'],
 'mods/cosmetics/src/mod.cpp':['sync_xbox_appearance();'],
 'src/dusk/imgui/PureItemTracker.cpp':['###GameTimerOverlayV1','###DeathCounterOverlayV1'],
}
for name,markers in required.items():
 text=(r/name).read_text()
 for marker in markers:
  if marker not in text:raise RuntimeError('.747 runtime missing '+marker+' in '+name)
assert identity['previousStartupJournals']==[previous['startupJournal']]+previous['previousStartupJournals']
originals={};prepared={}
for name in ['src/dusk/startup_guard.hpp','src/dusk/ui/prelaunch.cpp','mods/randomizer/src/session.cpp','platforms/uwp/Package.appxmanifest','src/dusk/xbox_memory_budget.hpp']:
 p=r/name;originals[p]=p.read_bytes();text=originals[p].decode().replace('\r\n','\n')
 if previous['version'] not in text or identity['version'] in text:raise RuntimeError('Unexpected .747 source identity: '+name)
 text=text.replace(previous['version'],identity['version'])
 if name=='src/dusk/startup_guard.hpp':
  old='detail::markerPath = userPath / "'+previous['startupJournal']+'";'
  journals='{'+', '.join(json.dumps(v) for v in previous['previousStartupJournals'])+'}'
  assert text.count(old)==1 and text.count(journals)==1
  text=text.replace(old,'detail::markerPath = userPath / "'+identity['startupJournal']+'";')
  text=text.replace(journals,'{'+', '.join(json.dumps(v) for v in identity['previousStartupJournals'])+'}')
 prepared[p]=text.encode()
try:
 for p,data in prepared.items():p.write_bytes(data)
except Exception:
 for p,data in originals.items():p.write_bytes(data)
 raise
print('Verified .747 companion API/capabilities and retained .744/.745/.746 layers; advanced release identities and preserved all startup journals')
