"""Create an isolated six-color SD test kit; no dependencies or device writes.

Usage: python tools/make_test_sd.py --output ../album-test-sd
Copy the generated configuration and directory to SD after preserving any old config.
"""
import argparse
import json
import struct
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    target = args.output.resolve()
    if target.exists() and any(target.iterdir()):
        parser.error('Output directory must be empty; existing files are never overwritten.')
    target.mkdir(parents=True, exist_ok=True)
    root = '/epaper-album-test'
    config = {
        'schema_version': 1, 'album': {'root': root, 'manifest': 'albums.json'},
        'display': {'rotation_degrees': 0, 'on_invalid_image': 'skip'},
        'announcement': {'enabled': False}, 'activity': {'enabled': False},
        'network': {'wifi_enabled': False},
        'power': {'sleep_between_events': True, 'low_battery_policy': 'none'},
    }
    manifest = {
        'schema_version': 1,
        'playback': {'enabled': True, 'default_interval_seconds': 120,
                     'order': 'sequential', 'album_strategy': 'album_by_album',
                     'recursive': False, 'resume_position': True},
        'albums': [{'id': 'test-a', 'directory': 'a', 'enabled': True},
                   {'id': 'test-b', 'directory': 'b', 'enabled': True},
                   {'id': 'test-disabled', 'directory': 'off', 'enabled': False}],
    }
    (target / 'config.json').write_text(json.dumps(config, indent=2)+'\n', encoding='utf-8')
    folder = target / root.lstrip('/')
    folder.mkdir()
    (folder / 'albums.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    colors = [(0, 0, 0), (255, 255, 255), (255, 255, 0),
              (255, 0, 0), (0, 0, 255), (0, 255, 0)]
    header = b'BM'+struct.pack('<IHHI', 1152054, 0, 0, 54)
    header += struct.pack('<IiiHHIIiiII', 40, 800, 480, 1, 24, 0, 1152000, 0, 0, 0, 0)
    for index, relative in enumerate(['a/001.bmp', 'a/002.bmp', 'b/001.bmp', 'off/001.bmp']):
        path = folder / relative
        path.parent.mkdir(exist_ok=True)
        with path.open('wb') as file:
            file.write(header)
            for file_y in range(480):
                y = 479-file_y
                row = bytearray()
                for x in range(800):
                    color = colors[(x*6//800+index) % 6]
                    if 10 <= x < 270 and 10 <= y < 70:
                        color = (255, 255, 255)
                    if 20 <= y < 60 and any(20+i*60 <= x < 60+i*60 for i in range(index+1)):
                        color = (0, 0, 0)
                    if x < 4 or x >= 796 or y < 4 or y >= 476:
                        color = (0, 0, 0)
                    row.extend((color[2], color[1], color[0]))
                file.write(row)
    print(f'Created test kit: {target}; interval 120 s; a/001 → a/002 → b/001 → …')


if __name__ == '__main__':
    main()
