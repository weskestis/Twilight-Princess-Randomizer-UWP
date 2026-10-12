"""Verify the restored source, then advance only the release identities."""
import hashlib,json,os
from pathlib import Path
r=Path(os.environ['TPR_SRC']);c=Path(os.environ['TPR_CONTROL'])
previous=json.loads((c/'APP_IDENTITY-744.json').read_text());identity=json.loads((c/'APP_IDENTITY-745.json').read_text())
assert previous['version']=='1.4.1.744' and identity['version']=='1.4.1.745'
for name,sha in identity['sourceControls'].items():
 if hashlib.sha256((c/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()!=sha:raise RuntimeError('.745 source control hash mismatch: '+name)
for name,sha in identity['sourcePatches'].items():
 if hashlib.sha256((c/'patches'/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()!=sha:raise RuntimeError('.745 patch hash mismatch: '+name)
required={
 'mods/cosmetics/src/mod.cpp':['gAppearanceRuntime.string(handle)','gAppearanceRuntime.integer(handle)','sync_xbox_appearance();','gAppearanceTargets.clear();','Randomize All Colors on Every Load Screen'],
 'mods/cosmetics/src/appearance_runtime.hpp':['HeartPalettes','random % 8 == 0','mode == Mode::Load && revision != revision_','milliseconds / 50'],
 'mods/cosmetics/src/midna_hair_color.cpp':['get_xbox_midna_rainbow_phase','xbox_selected_midna_color'],
 'src/dusk/ui/menu_colors.cpp':['sAppearanceRevision = value;','tpr_appearance_seed();','Randomize All Colors on Every Load Screen'],
 'src/dusk/imgui/PureItemTracker.cpp':['###GameTimerOverlayV1','###DeathCounterOverlayV1','ImVec4(1.0f, 0.20f, 0.20f, 1.0f)','tpr_run_overlay_centiseconds()','ImGuiWindowFlags_NoInputs'],
 'src/dusk/imgui/ImGuiConsole.cpp':['draw_run_overlays();'],
 'src/dusk/ui/settings.cpp':['Transparent Run Overlays','Game Timer Horizontal Position','Death Counter Vertical Position','Reset Game Timer Layout','Reset Death Counter Layout'],
 'mods/randomizer/src/session.cpp':['tpr_run_overlay_centiseconds','randomizer_GetActiveGameTimeCentiseconds()','tpr_appearance_seed'],
 'mods/randomizer/src/randomizer_context.cpp':['randomizer.active_game_time.v1','kSimulationFramesPerSecond = 30','randomizer_AdvanceActiveGameTimeFrame('],
}
for name,markers in required.items():
 text=(r/name).read_text()
 for marker in markers:
  if marker not in text:raise RuntimeError('.745 runtime missing '+marker+' in '+name)
assert identity['previousStartupJournals']==[previous['startupJournal']]+previous['previousStartupJournals']
originals={};prepared={}
for name in ['src/dusk/startup_guard.hpp','src/dusk/ui/prelaunch.cpp','mods/randomizer/src/session.cpp','platforms/uwp/Package.appxmanifest','src/dusk/xbox_memory_budget.hpp']:
 p=r/name;originals[p]=p.read_bytes();text=originals[p].decode().replace('\r\n','\n')
 if previous['version'] not in text or identity['version'] in text:raise RuntimeError('Unexpected .745 source identity: '+name)
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
print('Verified .745 appearance/overlay source, advanced package/launcher/save/runtime identities and retained every previous startup journal')
