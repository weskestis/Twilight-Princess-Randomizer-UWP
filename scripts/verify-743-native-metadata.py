"""Verify every signed native implementation in the final executable."""
import argparse,importlib.util,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('pe',type=Path);a=p.parse_args();control=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('pe_metadata',control/'scripts/verify-739-native-metadata.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
descriptors=[('','modmeta'),('cosmetics_','cosmeta'),('controller_ui_','cuimeta'),('luau_runtime_','luumeta')]
descriptors += [(m['target']+'_',m['metadataSection']) for m in json.loads((control/'NATIVE_MODS-743.json').read_text())['mods']]
module.verify(a.pe,descriptors)
