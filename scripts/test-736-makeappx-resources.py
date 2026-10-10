"""Use the real Windows SDK packer to check literal names before the game build."""
import argparse
import hashlib
import json
import shutil
import struct
import subprocess
import tempfile
import zipfile
import zlib
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('cache', type=Path)
args = parser.parse_args()
control = Path(__file__).resolve().parents[1]
lock = json.loads((control/'BUNDLED_MODS-736.json').read_text())
makeappx = shutil.which('makeappx')
compiler = shutil.which('cl')
if not makeappx or not compiler:
    raise RuntimeError('Windows SDK packer and MSVC are required')

def png(size):
    def chunk(kind, data):
        return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
    rows = (b'\0'+b'\x20\x40\x60'*size)*size
    return (b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',size,size,8,2,0,0,0))
            +chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b''))

with tempfile.TemporaryDirectory(prefix='tpr-736-makeappx-') as temp:
    work = Path(temp)
    root = work/'input';root.mkdir()
    source = work/'probe.cpp';source.write_text('int main() { return 0; }\n')
    subprocess.run([compiler,'/nologo',str(source),'/Fe:'+str(root/'Probe.exe')],cwd=work,check=True)
    for name,size in [('logo.png',50),('square150.png',150),('square44.png',44)]:
        (root/name).write_bytes(png(size))
    (root/'AppxManifest.xml').write_text('''<?xml version="1.0" encoding="utf-8"?>
<Package xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10"
 xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10"
 xmlns:rescap="http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities"
 IgnorableNamespaces="uap rescap">
 <Identity Name="TPRResourceProbe" Publisher="CN=TwilightPrincessRandomizer" Version="1.0.0.0" ProcessorArchitecture="x64"/>
 <Properties><DisplayName>Resource Probe</DisplayName><PublisherDisplayName>TPR</PublisherDisplayName><Logo>logo.png</Logo></Properties>
 <Dependencies><TargetDeviceFamily Name="Windows.Desktop" MinVersion="10.0.17763.0" MaxVersionTested="10.0.26100.0"/></Dependencies>
 <Resources><Resource Language="en-us"/></Resources>
 <Applications><Application Id="Probe" Executable="Probe.exe" EntryPoint="Windows.FullTrustApplication">
 <uap:VisualElements DisplayName="Resource Probe" Description="Resource Probe" BackgroundColor="transparent"
 Square150x150Logo="square150.png" Square44x44Logo="square44.png"/></Application></Applications>
 <Capabilities><rescap:Capability Name="runFullTrust"/></Capabilities>
</Package>''',encoding='utf-8')
    subprocess.run(['python',str(control/'scripts/complete-736-packaged-resources.py'),str(root),str(args.cache)],check=True)
    protected = hashlib.sha256((root/'Probe.exe').read_bytes()).hexdigest()
    dollar_files = [path for path in root.rglob('*') if path.is_file() and '$' in path.name]
    assert len(dollar_files)==2
    for path in dollar_files:path.unlink()
    subprocess.run(['python',str(control/'scripts/complete-736-packaged-resources.py'),str(root),str(args.cache)],check=True)
    assert hashlib.sha256((root/'Probe.exe').read_bytes()).hexdigest()==protected
    package = work/'probe.msix'
    subprocess.run([makeappx,'pack','/d',str(root),'/p',str(package),'/o','/h','SHA256','/l'],check=True)
    with zipfile.ZipFile(package) as packed:
        assert packed.read('Probe.exe')==(root/'Probe.exe').read_bytes()
        for mod in lock['mods']:
            if not mod['bundle']:continue
            folder='controller_ui' if mod['id']=='org.dusklight.tp_classic_buttons' else mod['id']
            with zipfile.ZipFile(args.cache/(mod['id']+'_'+mod['version']+'.dusk')) as original:
                for entry in original.infolist():
                    if entry.is_dir() or entry.filename.lower().endswith(('.dll','.so','.dylib')):continue
                    assert packed.read('mods/'+folder+'/'+entry.filename)==original.read(entry)
    print('PASS Windows MakeAppx: all 22 byte-exact resource sets, two literal dollar texture paths, protected executable and SHA256 block map')
