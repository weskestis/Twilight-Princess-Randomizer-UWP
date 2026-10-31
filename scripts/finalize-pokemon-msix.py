"""Privately add the verified Pikachu pack to the signed .749 preview build.

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
import tempfile
from urllib.parse import unquote
import xml.etree.ElementTree as ET
import zipfile
import zlib

FOOTPRINTS = {'AppxBlockMap.xml', '[Content_Types].xml', 'AppxSignature.p7x',
              'AppxMetadata/CodeIntegrity.cat'}
MODEL_PATH = 'mods/port_pokemon_model_lab/res/pikachu.dpkm'
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


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--input', type=Path, required=True)
    p.add_argument('--source-cert', type=Path, required=True)
    p.add_argument('--model', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--makemsix', type=Path, required=True)
    p.add_argument('--osslsigncode', type=Path, required=True)
    p.add_argument('--commit', required=True)
    p.add_argument('--version', default='1.4.1.750')
    a = p.parse_args()
    assert re.fullmatch('[a-f0-9]{40}', a.commit)
    assert re.fullmatch(r'\d+\.\d+\.\d+\.\d+', a.version)
    assert tuple(map(int, a.version.split('.'))) > (1, 4, 1, 749)
    assert all(int(x) <= 65535 for x in a.version.split('.'))
    for field in ['input', 'source_cert', 'model', 'output', 'makemsix', 'osslsigncode']:
        setattr(a, field, getattr(a, field).resolve())
    a.output.mkdir(parents=True, exist_ok=True)
    package = a.output / ('TwilightPrincessRandomizer_' + a.version + '_PikachuPreview_x64.msix')
    assert not package.exists(), 'Refusing to replace a delivered package'
    blob = a.model.read_bytes()
    assert blob[:8] == b'DPKM0001'
    length, crc, vertices, indices, batches, textures, clips = struct.unpack_from('<7I', blob, 8)
    assert length == len(blob) and crc == zlib.crc32(blob[16:])
    assert (vertices, indices, batches, textures, clips) == (2032, 11247, 15, 15, 15)
    del blob
    commands = []
    with tempfile.TemporaryDirectory(prefix='private-pikachu-package-', dir=a.output.parent) as temporary:
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
        assert mod['id'] == 'dev.weskestis.pokemon_model_lab' and mod['version'] == '0.3.0'
        control = Path(__file__).resolve().parents[1]
        run(['python', control / 'scripts/verify-pokemon-uwp.py', payload], 'verify-preview-native-metadata')
        # Change only the installer version; the complete executable remains the .749 build.
        assert xml.count('Version="1.4.1.749"') == 1
        manifest.write_text(xml.replace('Version="1.4.1.749"', 'Version="' + a.version + '"'), encoding='utf-8')
        shutil.copyfile(a.model, payload / MODEL_PATH)
        proof = {'version': a.version, 'baseVersion': '1.4.1.749', 'sourceCommit': a.commit,
                 'sourcePackageSha256': sha(a.input), 'modelSha256': sha(a.model),
                 'modelBytes': a.model.stat().st_size, 'triangles': indices // 3, 'clips': clips,
                 'hardwareTested': False, 'mode': 'animated model preview; combat not connected'}
        (payload / PROOF_PATH).write_text(json.dumps(proof, indent=2) + '\n')
        key, cert = work / 'key.pem', work / 'cert.pem'
        run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-sha256', '-nodes',
             '-keyout', key, '-out', cert, '-days', '365', '-subj', '/CN=TwilightPrincessRandomizer',
             '-addext', 'basicConstraints=critical,CA:FALSE', '-addext', 'keyUsage=critical,digitalSignature',
             '-addext', 'extendedKeyUsage=codeSigning'], 'create-preview-certificate')
        os.chmod(key, 0o600)
        public_cert = a.output / 'TwilightPrincessRandomizer-PikachuPreview.cer'
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
            run([signing, 'verify', '-CAfile', cert, '-in', signed_catalog], 'verify-preview-catalog')
            # The SDK excludes this reserved footprint when packing. Append it without
            # recompressing existing payload blocks; the signer includes its AXCI hash.
            append_catalog(unsigned, signed_catalog)
        run([signing, 'sign', '-certs', cert, '-key', key, '-h', 'sha256',
             '-in', unsigned, '-out', candidate], 'sign-preview-package')
        run([signing, 'verify', '-CAfile', cert, '-in', candidate], 'verify-preview-signature')
        after = inventory(candidate)
        assert set(after) == set(before) | {MODEL_PATH, PROOF_PATH}
        for name, expected in before.items():
            if name != 'AppxManifest.xml':
                assert after[name] == expected, 'Inherited .749 payload changed: ' + name
        assert after[MODEL_PATH]['sha256'] == proof['modelSha256']
        # Independent SDK unpack verifies the finalized block map and the installed names.
        checked = work / 'checked'
        run([a.makemsix, 'unpack', '-p', candidate, '-d', checked, '-ss'], 'verify-sdk-unpack')
        assert sha(checked / MODEL_PATH) == proof['modelSha256']
        for name, expected in after.items():
            assert sha(checked / name) == expected['sha256'], 'SDK installed filename/content mismatch: ' + name
        proof.update({'package': package.name, 'packageSha256': sha(candidate), 'packageBytes': candidate.stat().st_size,
                      'verifiedPayloadFiles': len(after), 'preservedBinaryPayload': True, 'checks': commands})
        os.replace(candidate, package)
        (a.output / 'PikachuPreview-Build.json').write_text(json.dumps(proof, indent=2) + '\n')
        (a.output / 'SHA256SUMS.txt').write_text(sha(package) + '  ' + package.name + '\n' + sha(public_cert) + '  ' + public_cert.name + '\n')
    print(json.dumps({'package': str(package), 'sha256': proof['packageSha256'], 'payloadFiles': len(after)}))


if __name__ == '__main__':
    main()
