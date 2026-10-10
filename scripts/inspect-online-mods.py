#!/usr/bin/env python3
"""Fetch and verify the complete catalog without executing any downloaded code."""
import argparse
import hashlib
import json
import re
import stat
import time
import urllib.parse
import urllib.request
import zipfile
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath

API = 'https://twilitrealm.dev/api/v1/games/dusklight'
HEADERS = {'User-Agent': 'Dusklight-UWP-Mod-Inventory/1.4.1.735',
           'X-Dusklight-Version': '2.0.0-alpha2', 'Accept': 'application/json'}
parser = argparse.ArgumentParser()
parser.add_argument('output', type=Path)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
packages = args.output / 'packages'
packages.mkdir(exist_ok=True)


def request(url):
    for attempt in range(3):
        try:
            return urllib.request.urlopen(urllib.request.Request(url, headers=HEADERS), timeout=60)
        except Exception:
            if attempt == 2:
                raise
            time.sleep(2 ** attempt)


def fetch_json(url):
    with request(url) as response:
        return json.load(response)


def inspect_package(path, detail):
    with zipfile.ZipFile(path) as bundle:
        names = []
        expanded = 0
        for entry in bundle.infolist():
            name = entry.filename
            canonical = PurePosixPath(name)
            if ('\\' in name or canonical.is_absolute() or '..' in canonical.parts
                    or ':' in name or '\x00' in name):
                raise ValueError(f'Unsafe archive entry: {name!r}')
            if stat.S_ISLNK(entry.external_attr >> 16):
                raise ValueError(f'Symbolic link archive entry: {name!r}')
            expanded += entry.file_size
            if entry.file_size > 512 * 1024 * 1024 or expanded > 2 * 1024 ** 3:
                raise ValueError('Archive expansion exceeds inventory limit')
            if not entry.is_dir():
                names.append(name)
        if len(names) != len(set(names)):
            raise ValueError('Duplicate archive entries')
        if len(names) != len({name.casefold() for name in names}):
            raise ValueError('Case-colliding archive entries')
        if 'mod.json' not in names:
            raise ValueError('Missing root mod.json')
        if bundle.getinfo('mod.json').file_size > 1024 * 1024:
            raise ValueError('Manifest is too large')
        manifest = json.loads(bundle.read('mod.json'))
        if manifest.get('id') != detail['id'] or manifest.get('version') != detail['version']:
            raise ValueError('Catalog identity does not match bundle manifest')
        native = [name for name in names if name.lower().endswith(('.dll', '.so', '.dylib'))]
        if bool(native) != bool(detail['contains_native_code']):
            raise ValueError('Catalog native-code flag does not match archive')
        corrupt = bundle.testzip()
        if corrupt:
            raise ValueError(f'Bad archive CRC: {corrupt}')
        return {'manifest': manifest, 'files': names, 'expanded_size': expanded,
                'native_entries': native, 'runtime': manifest.get('runtime'),
                'classification': 'native-port-required' if native else
                ('script-runtime-required' if manifest.get('runtime') else 'asset-package')}


records = []
seen = set()
page = 1
while True:
    catalog = fetch_json(API + '/mods?' + urllib.parse.urlencode(
        {'sort': 'downloads', 'page': page, 'include_natives': 'true'}))
    if catalog.get('game', {}).get('id') != 'dusklight':
        raise ValueError('Wrong catalog game')
    for mod in catalog['mods']:
        identifier = mod['id']
        if identifier in seen:
            raise ValueError('Duplicate catalog ID: ' + identifier)
        if not re.fullmatch(r'[a-z0-9_]+(?:\.[a-z0-9_]+)+', identifier):
            raise ValueError('Unexpected catalog ID')
        seen.add(identifier)
        detail = fetch_json(API + '/mods/' + urllib.parse.quote(identifier, safe=''))
        record = {'id': identifier, 'name': detail['name'], 'version': detail['version'],
                  'author': detail['author'], 'site_url': detail['site_url'],
                  'source_url': detail.get('source_url'), 'license': detail.get('license'),
                  'supported_platforms': detail.get('supported_platforms', []),
                  'service_imports': detail.get('service_imports', []),
                  'contains_native_code': detail['contains_native_code'],
                  'download': detail['download']}
        version = detail['version']
        if not re.fullmatch(r'[A-Za-z0-9.+_-]+', version):
            raise ValueError('Unsafe version identifier')
        target = packages / (identifier + '_' + version + '.dusk')
        download = detail['download']
        expected_hash = download['sha256'].lower()
        if not re.fullmatch(r'[a-f0-9]{64}', expected_hash):
            raise ValueError('Invalid SHA256 from catalog')
        if not isinstance(download['size'], int) or not 0 < download['size'] <= 512 * 1024 * 1024:
            raise ValueError('Invalid download size')
        url = download['url']
        if url.startswith('/'):
            url = urllib.parse.urljoin('https://twilitrealm.dev', url)
        parsed = urllib.parse.urlparse(url)
        if parsed.scheme != 'https' or parsed.hostname != 'twilitrealm.dev':
            raise ValueError('Unexpected download origin')
        try:
            hasher = hashlib.sha256()
            size = 0
            partial = target.with_suffix('.partial')
            with request(url) as response, partial.open('wb') as output:
                while block := response.read(1024 * 1024):
                    size += len(block)
                    if size > download['size']:
                        raise ValueError('Download exceeds published size')
                    hasher.update(block)
                    output.write(block)
            if size != download['size'] or hasher.hexdigest() != expected_hash:
                raise ValueError('Download hash or size mismatch')
            partial.replace(target)
            record.update(inspect_package(target, detail))
            record['package'] = str(target.relative_to(args.output))
            record['verified_sha256'] = hasher.hexdigest()
            print(f"VERIFIED {identifier} {version}: {record['classification']}", flush=True)
        except Exception as error:
            record['classification'] = 'validation-failed'
            record['error'] = str(error)
            print(f'FAILED {identifier}: {error}', flush=True)
        records.append(record)
    pagination = catalog['pagination']
    if page >= pagination['page_count']:
        if len(seen) != pagination['total']:
            raise ValueError('Catalog total changed during inventory; repeat with a consistent snapshot')
        break
    page += 1

counts = {}
for record in records:
    kind = record['classification']
    counts[kind] = counts.get(kind, 0) + 1
result = {'created_at': datetime.now(timezone.utc).isoformat(), 'catalog_url': API,
          'total': len(records), 'counts': counts, 'mods': records}
(args.output / 'inventory.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
print(json.dumps({'total': len(records), 'counts': counts}), flush=True)
if counts.get('validation-failed'):
    raise SystemExit('One or more packages failed verification; see inventory.json')
