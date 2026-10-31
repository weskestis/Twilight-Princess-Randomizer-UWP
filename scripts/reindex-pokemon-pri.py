"""Reindex .749's UWP resources under the separate preview package name.

Run with Windows SDK MakePri. The PRI indexer preserves resource names, values
and qualifiers while merging them into the explicitly supplied new map name.
https://learn.microsoft.com/windows/uwp/app-resources/makepri-exe-format-specific-indexers
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parent))
from pokemon_package_identity import BASE_NAME, PREVIEW_NAME, DISPLAY_NAME, PHONE_ID


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inventory(path, package):
    root = ET.parse(path).getroot()
    maps = root.findall('ResourceMap')
    assert len(maps) == 1 and maps[0].get('name') == package

    def canonical(node):
        attributes = sorted((key, value.replace('ms-resource://' + package + '/', 'ms-resource://PACKAGE/'))
                            for key, value in node.attrib.items() if key != 'index')
        text = (node.text or '').strip()
        children = sorted((canonical(child) for child in node), key=repr)
        return node.tag, attributes, text, children

    resources = sorted((canonical(node) for node in maps[0].iter('NamedResource')), key=repr)
    assert resources, 'Empty resource map'
    return resources


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-pri', type=Path, required=True)
    parser.add_argument('--makepri', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    tool = args.makepri.resolve()

    def run(*command):
        subprocess.run([str(tool), *map(str, command)], check=True)

    with tempfile.TemporaryDirectory(prefix='pokemon-pri-') as temp:
        work = Path(temp)
        shutil.copyfile(args.source_pri, work / 'source.pri')
        before = output / 'source.pri.xml'
        after = output / 'preview.pri.xml'
        run('dump', '/if', work / 'source.pri', '/of', before, '/dt', 'detailed', '/o')
        config = work / 'config.xml'
        run('createconfig', '/cf', config, '/dq', 'lang-en-US', '/pv', '10.0.0', '/o')
        tree = ET.parse(config)
        root = tree.getroot()
        packaging = root.find('packaging')
        if packaging is not None:
            root.remove(packaging)
        index = root.find('index')
        assert index is not None
        index.set('root', '.')
        index.set('startIndexAt', 'source.pri')
        for child in list(index):
            if child.tag != 'default':
                index.remove(child)
        ET.SubElement(index, 'indexer-config', {'type': 'PRI'})
        tree.write(config, encoding='utf-8', xml_declaration=True)
        result = output / 'resources.pri'
        run('new', '/pr', work, '/cf', config, '/in', PREVIEW_NAME, '/of', result, '/o')
        run('dump', '/if', result, '/of', after, '/dt', 'detailed', '/o')
        original = inventory(before, BASE_NAME)
        converted = inventory(after, PREVIEW_NAME)
        assert converted == original, 'Resource candidates changed while renaming the package map'
        proof = {'sourcePackageName': BASE_NAME, 'packageName': PREVIEW_NAME,
                 'displayName': DISPLAY_NAME, 'phoneProductId': PHONE_ID,
                 'sourcePriSha256': sha(args.source_pri), 'previewPriSha256': sha(result),
                 'resourceCount': len(original), 'allResourceCandidatesPreserved': True}
        (output / 'Separate-App-Identity.json').write_text(json.dumps(proof, indent=2) + '\n')
        print('PASS: separate package resource map and every original resource candidate preserved')


if __name__ == '__main__':
    main()
