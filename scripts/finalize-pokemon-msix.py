"""Privately add the verified 20-species content to the signed .749 preview build.

Uses Microsoft's MSIX SDK for package/block-map creation and osslsigncode for
Authenticode signing. No retail data or signing keys belong in the repository.
"""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from urllib.parse import unquote
import xml.etree.ElementTree as ET
import zipfile
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parent))
from pokemon_package_identity import BASE_NAME, PREVIEW_NAME, DISPLAY_NAME, PHONE_ID, preview_manifest

FOOTPRINTS = {'AppxBlockMap.xml', '[Content_Types].xml', 'AppxSignature.p7x',
              'AppxMetadata/CodeIntegrity.cat'}
CONTENT_PATH = 'mods/port_pokemon_model_lab/res/pokemon'
PROOF_PATH = 'mods/port_pokemon_model_lab/res/preview-build.json'
NS = {'b': 'http://schemas.microsoft.com/appx/2010/blockmap',
      'm': 'http://schemas.microsoft.com/appx/manifest/foundation/windows10'}


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def safe_name(name):
    name = name.replace('\\', '/')
    p = PurePosixPath(name)
    if p.is_absolute() or '..' in p.parts or ':' in name or '\x00' in name:
        raise ValueError('Unsafe package path: ' + name)
    return name


def catalog_content(data):
    """Extract opaque PKCS#7 signed content for equality, not trust validation."""
    def tlv(offset):
        start = offset
        assert offset + 2 <= len(data)
        tag, size = data[offset], data[offset + 1]
        offset += 2
        if size & 128:
            count = size & 127
            assert 0 < count <= 8 and offset + count <= len(data)
            size = int.from_bytes(data[offset:offset + count], 'big')
            offset += count
        end = offset + size
        assert end <= len(data)
        return tag, start, offset, end

    def children(node):
        result, offset = [], node[2]
        while offset < node[3]:
            item = tlv(offset)
            assert item[3] <= node[3]
            result.append(item)
            offset = item[3]
        return result

    root = tlv(0)
    assert root[0] == 0x30 and root[3] == len(data)
    top = children(root)
    assert len(top) == 2 and top[1][0] == 0xa0
    assert data[top[0][1]:top[0][3]] == bytes.fromhex('06092a864886f70d010702')
    signed = children(top[1])
    assert len(signed) == 1 and signed[0][0] == 0x30
    fields = children(signed[0])
    assert len(fields) >= 4 and fields[2][0] == 0x30
    return data[fields[2][1]:fields[2][3]]


def append_catalog(package, catalog):
    """Preserve the SDK's 64-bit data descriptors when adding the CAT footprint."""
    limit = zipfile.ZIP64_LIMIT
    try:
        zipfile.ZIP64_LIMIT = 0
        with zipfile.ZipFile(package, 'a', compression=zipfile.ZIP_DEFLATED) as archive:
            assert 'AppxMetadata/CodeIntegrity.cat' not in archive.namelist()
            assert not archive.comment
            archive.write(catalog, 'AppxMetadata/CodeIntegrity.cat')
    finally:
        zipfile.ZIP64_LIMIT = limit
    # Python emits a ZIP64 EOCD/locator but leaves ordinary EOCD values when
    # they fit in 32 bits. Explicit sentinels retain the SDK's all-ZIP64 form;
    # otherwise readers may interpret its 64-bit data descriptors as 32-bit.
    with Path(package).open('r+b') as stream:
        stream.seek(-98, 2)
        trailer = stream.read(98)
        assert trailer[:4] == b'PK\x06\x06' and trailer[56:60] == b'PK\x06\x07'
        assert trailer[76:80] == b'PK\x05\x06' and trailer[-2:] == b'\x00\x00'
        end = bytearray(trailer[76:])
        struct.pack_into('<HHII', end, 8, 65535, 65535, 0xffffffff, 0xffffffff)
        stream.seek(-22, 2)
        stream.write(end)


def inventory(package, extract=None):
    """Verify every uncompressed 64 KiB block and its installed (OPC-decoded) name."""
    with zipfile.ZipFile(package) as archive:
        entries, names = {}, set()
        for info in archive.infolist():
            if info.is_dir():
                continue
            name = safe_name(unquote(info.filename, errors='strict'))
            if name.casefold() in names:
                raise ValueError('Duplicate installed package path: ' + name)
            names.add(name.casefold())
            entries[name] = info
        blockmap = ET.fromstring(archive.read(entries['AppxBlockMap.xml']))
        assert blockmap.get('HashMethod') == 'http://www.w3.org/2001/04/xmlenc#sha256'
        verified = {}
        for file in blockmap.findall('b:File', NS):
            name = safe_name(file.attrib['Name'])
            assert name not in verified, 'Duplicate block-map entry'
            info = entries[name]
            size = int(file.attrib['Size'])
            assert size == info.file_size, 'Block-map length mismatch: ' + name
            digest = hashlib.sha256()
            count = 0
            with archive.open(info) as stream:
                for block in file.findall('b:Block', NS):
                    data = stream.read(65536)
                    assert hashlib.sha256(data).digest() == base64.b64decode(block.attrib['Hash'], validate=True), 'Block hash mismatch: ' + name
                    count += len(data)
                    digest.update(data)
                assert count == size and not stream.read(1), 'Block-map coverage mismatch: ' + name
            verified[name] = {'size': size, 'sha256': digest.hexdigest()}
        assert set(entries) - set(verified) == set(entries) & FOOTPRINTS, 'Payload missing from block map'
        if extract:
            for name, info in entries.items():
                path = extract / name
                path.parent.mkdir(parents=True, exist_ok=True)
                with archive.open(info) as source, path.open('wb') as out:
                    shutil.copyfileobj(source, out)
        return verified


def omit_optional_content(payload, before):
    """Omit complete, disabled-by-default CP20 content mods, never native code."""
    catalog_name = 'mods/CUSTOM-MODS-743.json'
    catalog_path = payload / catalog_name
    catalog = json.loads(catalog_path.read_text(encoding='utf-8-sig'))
    mods = catalog['mods']
    assert catalog['activationDefault'] == 'disabled' and len(mods) == 35
    ids = {mod['id'] for mod in mods}
    assert len(ids) == 35 and all(value.startswith('tpr.optional.') for value in ids)
    folders = {path.name: path for path in (payload / 'mods').glob('tpr.optional.*')}
    assert set(folders) == ids
    omitted = {name: item for name, item in before.items()
               if name.startswith('mods/') and name.split('/')[1] in ids}
    assert omitted and not any(PurePosixPath(name).suffix.lower() in {'.exe', '.dll', '.dusk'} for name in omitted)
    # No retained mod is allowed to depend on one of the omitted content mods.
    for manifest in (payload / 'mods').glob('*/mod.json'):
        if manifest.parent.name in ids:
            continue
        data = json.loads(manifest.read_text(encoding='utf-8-sig'))
        for key in ('dependencies', 'requires', 'depends'):
            assert not any(value in json.dumps(data.get(key, [])) for value in ids), manifest
    for folder in folders.values():
        shutil.rmtree(folder)
    catalog['mods'] = []
    catalog['expectedSelectableMods'] = 0
    catalog['expectedTextureFiles'] = 0
    catalog['expectedOverlayFiles'] = 0
    catalog['omittedForCompactPokemonPackage'] = [{'id': mod['id'], 'name': mod['name']} for mod in mods]
    catalog_path.write_text(json.dumps(catalog, indent=2) + '\n')
    changed = {catalog_name}
    thumbnail_name = 'mods/UI-THUMBNAILS-744.json'
    thumbnails_path = payload / thumbnail_name
    thumbnails = json.loads(thumbnails_path.read_text(encoding='utf-8-sig'))
    kept = [image for image in thumbnails['images'] if image['mod'] not in ids]
    if kept != thumbnails['images']:
        thumbnails['images'] = kept
        thumbnails_path.write_text(json.dumps(thumbnails, indent=2) + '\n')
        changed.add(thumbnail_name)
    bundled = json.loads((payload / 'mods/BUNDLED-MODS.json').read_text(encoding='utf-8-sig'))
    assert not any(mod['id'] in ids for mod in bundled['mods']), 'Unexpected dependency in the main bundled catalog'
    return omitted, changed, catalog['omittedForCompactPokemonPackage']


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--input', type=Path, required=True)
    p.add_argument('--source-cert', type=Path, required=True)
    p.add_argument('--content', type=Path, required=True)
    p.add_argument('--validator', type=Path, required=True, help='Built production pokemon_mode_tests executable')
    p.add_argument('--identity-dir', type=Path, required=True,
                   help='Verified Windows MakePri output for the separate preview app')
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--makemsix', type=Path, required=True)
    p.add_argument('--osslsigncode', type=Path, required=True)
    p.add_argument('--commit', required=True)
    p.add_argument('--version', default='1.4.1.754')
    p.add_argument('--compact', action='store_true', help='Omit the 35 disabled optional CP20 music/costume content packs')
    a = p.parse_args()
    assert re.fullmatch('[a-f0-9]{40}', a.commit)
    assert re.fullmatch(r'\d+\.\d+\.\d+\.\d+', a.version)
    assert tuple(map(int, a.version.split('.'))) > (1, 4, 1, 749)
    assert all(int(x) <= 65535 for x in a.version.split('.'))
    for field in ['input', 'source_cert', 'content', 'validator', 'identity_dir', 'output', 'makemsix', 'osslsigncode']:
        setattr(a, field, getattr(a, field).resolve())
    a.output.mkdir(parents=True, exist_ok=True)
    package = a.output / (PREVIEW_NAME + '_' + a.version + '_x64' + ('_Compact' if a.compact else '') + '.msix')
    assert not package.exists(), 'Refusing to replace a delivered package'
    control = Path(__file__).resolve().parents[1]
    roster = json.loads((control / 'experimental/pokemon_battle/assets/roster.json').read_text())['species']
    expected_names = {f"{species['id']:03d}.dpkm" for species in roster} | {'effects.dpfx'}
    assert len(roster) == 20 and len(expected_names) == 21
    content_files = {}; content_proof = {}; total_clips = 0
    for name in sorted(expected_names):
        source = a.content / name
        blob = source.read_bytes()
        assert blob[:8] == (b'DPFX0001' if name.endswith('.dpfx') else b'DPKM0002')
        length, crc = struct.unpack_from('<II', blob, 8)
        assert length == len(blob) and crc == zlib.crc32(blob[16:])
        assert length < (4 if name.endswith('.dpfx') else 40) * 1024 * 1024
        item = {'bytes': len(blob), 'sha256': sha(source)}
        if name.endswith('.dpkm'):
            vertices, indices, batches, textures, clips = struct.unpack_from('<5I', blob, 16)
            item.update(vertices=vertices, triangles=indices // 3, batches=batches, textures=textures, clips=clips)
            total_clips += clips
        destination = CONTENT_PATH + '/' + name
        content_files[destination] = source; content_proof[destination] = item
    del blob
    subprocess.run([str(a.validator), str(a.content)], check=True)
    commands = []
    with tempfile.TemporaryDirectory(prefix='private-pokemon20-package-', dir=a.output.parent) as temporary:
        work = Path(temporary)
        os.chmod(work, 0o700)
        candidate = work / package.name
        payload = work / 'payload'
        payload.mkdir()

        def run(args, label):
            commands.append(label)
            with (a.output / (label + '.log')).open('w') as log:
                subprocess.run([str(x) for x in args], stdout=log, stderr=subprocess.STDOUT, check=True)

        signing = a.osslsigncode
        source_cert = work / 'source.pem'
        run(['openssl', 'x509', '-inform', 'DER', '-in', a.source_cert, '-out', source_cert], 'read-source-certificate')
        run([signing, 'verify', '-CAfile', source_cert, '-in', a.input], 'verify-source-signature')
        before = inventory(a.input, payload)
        manifest = payload / 'AppxManifest.xml'
        xml = manifest.read_text(encoding='utf-8-sig')
        identity = ET.fromstring(xml).find('m:Identity', NS)
        assert identity is not None
        assert identity.attrib['Name'] == 'TwilightPrincessRandomizer'
        assert identity.attrib['Publisher'] == 'CN=TwilightPrincessRandomizer'
        assert identity.attrib['Version'] == '1.4.1.749'
        assert identity.attrib['ProcessorArchitecture'] == 'x64'
        mod = json.loads((payload / 'mods/port_pokemon_model_lab/mod.json').read_text())
        control = Path(__file__).resolve().parents[1]
        expected_mod = json.loads((control / 'experimental/pokemon_battle/model_lab/mod.json').read_text())
        assert mod == expected_mod, 'Preview implementation and resource bundle versions differ'
        run(['python', control / 'scripts/verify-pokemon-uwp.py', payload], 'verify-preview-native-metadata')
        omitted, changed_catalogs, omitted_mods = {}, set(), []
        if a.compact:
            omitted, changed_catalogs, omitted_mods = omit_optional_content(payload, before)
        identity_proof = json.loads((a.identity_dir / 'Separate-App-Identity.json').read_text())
        assert identity_proof['sourcePackageName'] == BASE_NAME
        assert identity_proof['packageName'] == PREVIEW_NAME
        assert identity_proof['displayName'] == DISPLAY_NAME and identity_proof['phoneProductId'] == PHONE_ID
        assert identity_proof['allResourceCandidatesPreserved'] and identity_proof['resourceCount'] > 0
        assert identity_proof['sourcePriSha256'] == sha(payload / 'resources.pri'), 'New build needs Windows PRI reindexing'
        assert identity_proof['previewPriSha256'] == sha(a.identity_dir / 'resources.pri')
        # Change the installation family as well as its visible name. Reindexing the
        # PRI is required: Windows checks its resource map against the package Name.
        manifest.write_text(preview_manifest(xml, a.version), encoding='utf-8')
        shutil.copyfile(a.identity_dir / 'resources.pri', payload / 'resources.pri')
        for destination, source in content_files.items():
            (payload / destination).parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, payload / destination)
        proof = {'version': a.version, 'baseVersion': '1.4.1.749', 'sourceCommit': a.commit,
                 'compact': a.compact, 'omittedOptionalMods': omitted_mods, 'omittedOptionalFiles': omitted,
                 'finalizerSha256': sha(Path(__file__)),
                 'identityCodeSha256': sha(control / 'scripts/pokemon_package_identity.py'),
                 'packageName': PREVIEW_NAME, 'sourcePackageName': BASE_NAME, 'separateApp': True,
                 'displayName': DISPLAY_NAME, 'phoneProductId': PHONE_ID, 'resourceIdentity': identity_proof,
                 'sourcePackageSha256': sha(a.input), 'content': content_proof,
                 'species': [species['name'] for species in roster], 'clips': total_clips,
                 'contentBytes': sum(item['bytes'] for item in content_proof.values()),
                 'hardwareTested': False, 'mode': 'Pokemon Vanilla and Pokemon Randomizer with 20 species, real-time encounters, animated balls and source-texture moves'}
        (payload / PROOF_PATH).write_text(json.dumps(proof, indent=2) + '\n')
        key, cert = work / 'key.pem', work / 'cert.pem'
        run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-sha256', '-nodes',
             '-keyout', key, '-out', cert, '-days', '365', '-subj', '/CN=TwilightPrincessRandomizer',
             '-addext', 'basicConstraints=critical,CA:FALSE', '-addext', 'keyUsage=critical,digitalSignature',
             '-addext', 'extendedKeyUsage=codeSigning'], 'create-preview-certificate')
        os.chmod(key, 0o600)
        public_cert = a.output / 'DusklightPokemonPreview.cer'
        run(['openssl', 'x509', '-in', cert, '-outform', 'DER', '-out', public_cert], 'export-preview-certificate')
        unsigned = work / 'unsigned.msix'
        run([a.makemsix, 'pack', '-d', payload, '-p', unsigned], 'pack-preview')
        catalog = payload / 'AppxMetadata/CodeIntegrity.cat'
        if catalog.exists():
            # The binaries do not change. Preserve the catalog's hashes and re-sign it
            # with the same certificate as the new outer package.
            signed_catalog = work / 'CodeIntegrity.cat'
            run([signing, 'sign', '-certs', cert, '-key', key, '-h', 'sha256',
                 '-in', catalog, '-out', signed_catalog], 'sign-preview-catalog')
            assert catalog_content(catalog.read_bytes()) == catalog_content(signed_catalog.read_bytes()), 'Code-integrity catalog contents changed'
            proof['catalogSignedContentPreserved'] = True
            run([signing, 'verify', '-CAfile', cert, '-in', signed_catalog], 'verify-preview-catalog')
            # The SDK excludes this reserved footprint when packing. Append it without
            # recompressing existing payload blocks; the signer includes its AXCI hash.
            append_catalog(unsigned, signed_catalog)
        run([signing, 'sign', '-certs', cert, '-key', key, '-h', 'sha256',
             '-in', unsigned, '-out', candidate], 'sign-preview-package')
        run([signing, 'verify', '-CAfile', cert, '-in', candidate], 'verify-preview-signature')
        after = inventory(candidate)
        assert set(after) == (set(before) - set(omitted)) | set(content_files) | {PROOF_PATH}
        for name, expected in before.items():
            if name not in {'AppxManifest.xml', 'resources.pri'} | changed_catalogs | set(omitted):
                assert after[name] == expected, 'Inherited .749 payload changed: ' + name
        for destination, item in content_proof.items():
            assert after[destination]['sha256'] == item['sha256']
        assert after['resources.pri']['sha256'] == identity_proof['previewPriSha256']
        # Independent SDK unpack verifies the finalized block map and the installed names.
        checked = work / 'checked'
        run([a.makemsix, 'unpack', '-p', candidate, '-d', checked, '-ss'], 'verify-sdk-unpack')
        for name, expected in after.items():
            assert sha(checked / name) == expected['sha256'], 'SDK installed filename/content mismatch: ' + name
        proof.update({'package': package.name, 'packageSha256': sha(candidate), 'packageBytes': candidate.stat().st_size,
                      'verifiedPayloadFiles': len(after), 'preservedBinaryPayload': True, 'checks': commands})
        os.replace(candidate, package)
        (a.output / 'Pokemon20-Build.json').write_text(json.dumps(proof, indent=2) + '\n')
        (a.output / 'SHA256SUMS.txt').write_text(sha(package) + '  ' + package.name + '\n' + sha(public_cert) + '  ' + public_cert.name + '\n')
    print(json.dumps({'package': str(package), 'sha256': proof['packageSha256'], 'payloadFiles': len(after)}))


if __name__ == '__main__':
    main()
