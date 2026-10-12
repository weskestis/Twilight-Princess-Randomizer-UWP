"""Verify the restored source, then advance only the release identities."""
import hashlib,json,os
from pathlib import Path
r=Path(os.environ['TPR_SRC']);c=Path(os.environ['TPR_CONTROL'])
previous=json.loads((c/'APP_IDENTITY-745.json').read_text());identity=json.loads((c/'APP_IDENTITY-746.json').read_text())
assert previous['version']=='1.4.1.745' and identity['version']=='1.4.1.746'
for name,sha in identity['sourceControls'].items():
 if hashlib.sha256((c/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()!=sha:raise RuntimeError('.746 source control hash mismatch: '+name)
for name,sha in identity['sourcePatches'].items():
 if hashlib.sha256((c/'patches'/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()!=sha:raise RuntimeError('.746 patch hash mismatch: '+name)
required={
 'src/dusk/ui/mods_window.cpp':['class ModListPane final','class ModDetailPane final','std::chrono::milliseconds(180)','commit_detail(true);','commit_panel(true);','mDisplayedModId != id','mRoot->SetProperty("visibility", "hidden")','if (mCulled) return;'],
 'src/dusk/ui/mods_window.hpp':['void show() override;','mPendingDetailPane','mDisplayedModId'],
 'src/dusk/ui/pane.cpp':['bool clearNextPane','if (clearNextPane) nextPane.clear();'],
 'src/dusk/ui/pane.hpp':['bool clearNextPane = true'],
 'src/dusk/ui/mod_panel_delay.hpp':['bool force = false','if (!force && now < mDue)'],
 'mods/cosmetics/src/mod.cpp':['gAppearanceRuntime.string(handle)','sync_xbox_appearance();'],
 'src/dusk/imgui/PureItemTracker.cpp':['###GameTimerOverlayV1','###DeathCounterOverlayV1'],
}
for name,markers in required.items():
 text=(r/name).read_text()
 for marker in markers:
  if marker not in text:raise RuntimeError('.746 runtime missing '+marker+' in '+name)
assert identity['previousStartupJournals']==[previous['startupJournal']]+previous['previousStartupJournals']
originals={};prepared={}
for name in ['src/dusk/startup_guard.hpp','src/dusk/ui/prelaunch.cpp','mods/randomizer/src/session.cpp','platforms/uwp/Package.appxmanifest','src/dusk/xbox_memory_budget.hpp']:
 p=r/name;originals[p]=p.read_bytes();text=originals[p].decode().replace('\r\n','\n')
 if previous['version'] not in text or identity['version'] in text:raise RuntimeError('Unexpected .746 source identity: '+name)
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
print('Verified .746 navigation source and retained .745 appearance/overlays, advanced package/launcher/save/runtime identities and retained every previous startup journal')
