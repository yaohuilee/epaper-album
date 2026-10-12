"""Back up and replace the SD album library through the device HTTP APIs.

Requires all new BMPs to be ready before any destructive operation.
Photos and metadata backups stay on the PC, outside the firmware repository.
"""
import argparse
import hashlib
import json
import time
from pathlib import Path
from urllib.parse import urlencode
from sd_photo_api import Api


def digest(data):
    return hashlib.sha256(data).hexdigest()


def photo_path(album, name):
    return '/api/photo?'+urlencode({'album': album, 'name': name})


def file_list(api, album):
    result = []
    offset = 0
    while True:
        api.wait_ready()
        page = api.json('/api/files?'+urlencode({'album': album, 'offset': offset, 'limit': 100}))
        result.extend(page['files'])
        if page['next_offset'] is None:
            return result
        offset = page['next_offset']


def backup(api, target):
    target.mkdir(parents=True, exist_ok=True)
    record = target/'backup.json'
    if record.exists():
        data = json.loads(record.read_text(encoding='utf-8'))
        for row in data['photos']:
            assert digest((target/row['local']).read_bytes()) == row['sha256']
        return data
    api.wait_ready()
    albums = api.json('/api/albums')
    data = {'library': albums, 'photos': []}
    for album in albums['albums']:
        if not album['id'] or any(c in album['id'] for c in '/\\') or album['id'] in ('.', '..'):
            raise ValueError('Album ID cannot be used as a local backup directory')
        for entry in file_list(api, album['id']):
            local = Path(album['id'])/entry['name']
            destination = target/local
            content = api.download_bytes(photo_path(album['id'], entry['name']), entry['bytes'])
            if len(content) != entry['bytes']:
                raise RuntimeError('Backup size mismatch: '+entry['name'])
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(content)
            row = dict(album=album['id'], name=entry['name'], local=local.as_posix(), bytes=len(content), sha256=digest(content))
            assert digest(destination.read_bytes()) == row['sha256']
            data['photos'].append(row)
            print('BACKUP', len(data['photos']), album['id'], entry['name'], flush=True)
    record.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf-8')
    return data


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--url', required=True)
    p.add_argument('--source', type=Path)
    p.add_argument('--backup', type=Path, required=True)
    p.add_argument('--backup-only', action='store_true')
    p.add_argument('--resume', action='store_true', help='Verify and continue a partially uploaded replacement without deleting files')
    args = p.parse_args()
    api = Api(args.url)
    saved = backup(api, args.backup)
    if args.backup_only:
        print('BACKUP_COMPLETE', len(saved['photos']), flush=True)
        return
    if not args.source:
        p.error('--source is required for replacement')
    subjects = json.loads((args.source/'subjects.json').read_text(encoding='utf-8'))
    assert len(subjects) == 100
    for item in subjects:
        data = (args.source/'bmp'/(item['name']+'.bmp')).read_bytes()
        metadata = json.loads((args.source/'bmp'/(item['name']+'.json')).read_text(encoding='utf-8'))
        assert digest(data) == metadata['sha256'] and len(data) == 1152054
    regions = list(dict.fromkeys(s['region'] for s in subjects))
    old = api.json('/api/albums')['albums']
    if args.resume:
        expected = {'world-'+r for r in regions}
        if ({a['id'] for a in old} != expected or
                any(a['enabled'] or a['directory'] != a['id'] for a in old)):
            raise RuntimeError('Resume requires exactly the five disabled target albums')
        expected_names = {r: {s['name']+'.bmp' for s in subjects if s['region'] == r} for r in regions}
        for r in regions:
            if any(e['name'] not in expected_names[r] for e in file_list(api, 'world-'+r)):
                raise RuntimeError('Unexpected file in target album; no mutation sent')
    # Extend the old deadline so deleting other files cannot advance onto them.
    elif old:
        api.command('/api/settings', {'schema_version': 1, 'interval_seconds': 604800})
    status = api.wait_ready()
    current = status['album']['current_photo']
    rows = []
    for album in ([] if args.resume else old):
        for entry in file_list(api, album['id']):
            sd_path = saved['library']['root'].rstrip('/')+'/'+album['directory']+'/'+entry['name']
            rows.append((sd_path == current, album['id'], entry['name']))
    # Delete the displayed photo last; other deletions preserve the panel/deadline.
    for _, album, name in sorted(rows):
        status = api.wait_ready()
        previous = status['control']['files_revision']
        api.json(photo_path(album, name), 'DELETE')
        api.wait_index(previous)
        print('DELETE_PHOTO', album, name, flush=True)
    for album in ([] if args.resume else old):
        api.command('/api/album-delete', {'schema_version': 1, 'id': album['id']})
        print('DELETE_ALBUM', album['id'], flush=True)
    if not args.resume:
        assert not api.json('/api/albums')['albums']
    for region in ([] if args.resume else regions):
        api.command('/api/album-create', {'schema_version': 1, 'id': 'world-'+region, 'enabled': False, 'interval_seconds': 900})
        print('CREATE_ALBUM', 'world-'+region, flush=True)
    installed_names = {r: {e['name'] for e in file_list(api, 'world-'+r)} for r in regions}
    for item in subjects:
        album = 'world-'+item['region']
        name = item['name']+'.bmp'
        content = (args.source/'bmp'/name).read_bytes()
        if name not in installed_names[item['region']]:
            status = api.wait_ready()
            previous = status['control']['files_revision']
            api.json(photo_path(album, name), 'PUT', content, 'image/bmp')
            api.wait_index(previous)
        installed = api.download_bytes(photo_path(album, name), len(content))
        if digest(installed) != digest(content):
            raise RuntimeError('Uploaded photo readback hash mismatch: '+name)
        print('UPLOAD_VERIFIED', item['index'], album, name, flush=True)
    # Enable only when all 100 files have passed firmware validation and readback.
    api.command('/api/settings', {'schema_version': 1, 'album_ids': ['world-'+r for r in regions], 'interval_seconds': 900})
    status = api.wait_ready()
    assert status['album']['photos'] == 100
    listing = {r: file_list(api, 'world-'+r) for r in regions}
    assert all(len(entries) == 20 and all(e['header_valid'] for e in entries) for entries in listing.values())
    result = {'version': status['device']['version'], 'albums': len(regions), 'photos': status['album']['photos'],
              'current_photo': status['album']['current_photo'], 'readback_verified': 100, 'listing': listing}
    (args.source/'installation.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print('INSTALLATION_COMPLETE', result['albums'], result['photos'], flush=True)


if __name__ == '__main__':
    main()
