"""One-time compatibility proof for the private Xbox model packaging pipeline."""
import importlib.util
from pathlib import Path
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('packaging',root/'scripts/finalize-pokemon-msix.py')
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
inventory=module.inventory(root/'tests/fixtures/msix-signing/probe.msix')
for name,expected in inventory.items():
    assert module.sha(root/'probe-unpacked'/name)==expected['sha256'],name
assert (root/'probe-unpacked/literal$atlas%25.txt').read_bytes()==bytes(range(256))*1000
print('PASS Microsoft Signtool signatures, Microsoft MakeAppx extraction, every payload hash and literal dollar/percent filename')

# MakeAppx deliberately omits this reserved footprint when unpacking.
import zipfile
with zipfile.ZipFile(root/'tests/fixtures/msix-signing/probe.msix') as archive:
    (root/'probe-code-integrity.cat').write_bytes(archive.read('AppxMetadata/CodeIntegrity.cat'))
