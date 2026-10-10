"""Advance the complete green .740 source with reviewed generation/reset/save fixes."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

root=Path(os.environ['TPR_SRC']);control=Path(os.environ['TPR_CONTROL'])
identity_files=['src/dusk/startup_guard.hpp','src/dusk/ui/prelaunch.cpp',
 'mods/randomizer/src/session.cpp','platforms/uwp/Package.appxmanifest','src/dusk/xbox_memory_budget.hpp']
for name in identity_files:
 if '1.4.1.740' not in (root/name).read_text(encoding='utf-8'):
  raise RuntimeError('.741 requires the complete .740 source: '+name)
identity=json.loads((control/'APP_IDENTITY-741.json').read_text(encoding='utf-8'))
originals={};prepared=[]
with tempfile.TemporaryDirectory(prefix='tpr-741-patch-') as temporary:
 for kind,sub in [('host',''),('randomizer','mods/randomizer'),('aurora','extern/aurora')]:
  name=f'xbox-generation-reset-save-741-{kind}.patch';payload=(control/'patches'/name).read_bytes().replace(b'\r\n',b'\n')
  if hashlib.sha256(payload).hexdigest()!=identity['sourcePatches'][name]:raise RuntimeError('.741 patch hash mismatch: '+name)
  destination=root/sub
  for line in payload.decode('utf-8').splitlines():
   if not line.startswith('+++ b/'):continue
   path_name=line[6:]
   if path_name.startswith('/') or '..' in Path(path_name).parts:raise RuntimeError('.741 unsafe patch path')
   path=destination/path_name
   if path.exists():
    before=path.read_bytes()
    if before!=before.replace(b'\r\n',b'\n'):originals[path]=before;path.write_bytes(before.replace(b'\r\n',b'\n'))
  patch=Path(temporary)/name;patch.write_bytes(payload);prepared.append((destination,patch))
 try:
  for destination,patch in prepared:subprocess.run(['git','-C',str(destination),'apply','--check',str(patch)],check=True)
 except Exception:
  for path,before in originals.items():path.write_bytes(before)
  raise
 for destination,patch in prepared:subprocess.run(['git','-C',str(destination),'apply',str(patch)],check=True)
for name in identity_files:
 path=root/name;text=path.read_text(encoding='utf-8').replace('1.4.1.740','1.4.1.741')
 if name=='src/dusk/startup_guard.hpp':
  current='detail::markerPath = userPath / "xbox-startup-stage-740.txt";'
  previous='{"xbox-startup-stage-739.txt", "xbox-startup-stage-738.txt", "xbox-startup-stage-737.txt", "xbox-startup-stage-736.txt", "xbox-startup-stage-735.txt", "xbox-startup-stage-734.txt", "xbox-startup-stage-733.txt", "xbox-startup-stage-732.txt"}'
  if text.count(current)!=1 or text.count(previous)!=1:raise RuntimeError('.741 startup journal anchors changed')
  text=text.replace(current,'detail::markerPath = userPath / "xbox-startup-stage-741.txt";').replace(previous,'{"xbox-startup-stage-740.txt", '+previous[1:])
 path.write_text(text,encoding='utf-8',newline='\n')
print('Applied .741 logical Max hints, cancel/retry UI lifecycle, black-fader native reset, missing vanilla Slot A and UTF-8/backend match; retained complete .740 and 67 strict scenarios.')
