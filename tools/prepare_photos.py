"""Resize and dither photos into the firmware's exact six-color 24-bit BMP format.

Requires Pillow: python -m pip install Pillow
Usage: python tools/prepare_photos.py originals output --preview-dir previews
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path
from PIL import Image, ImageOps, ImageEnhance, ImageFilter

COLORS = [(0, 0, 0), (255, 255, 255), (255, 255, 0),
          (255, 0, 0), (0, 0, 255), (0, 255, 0)]


def prepare(source, target, size, preview=None):
    if target.exists():
        raise FileExistsError(f'Existing output is never overwritten: {target}')
    with Image.open(source) as original:
        image = ImageOps.exif_transpose(original).convert('RGBA')
        background = Image.new('RGBA', image.size, 'white')
        image = Image.alpha_composite(background, image).convert('RGB')
        image = ImageOps.fit(image, size, method=Image.Resampling.LANCZOS)
        # Mild adjustment before quantization; never resample the dithered result.
        image = ImageOps.autocontrast(image, cutoff=0.4, preserve_tone=True)
        image = ImageEnhance.Contrast(image).enhance(1.06)
        image = image.filter(ImageFilter.UnsharpMask(radius=0.65, percent=90, threshold=3))
        palette = Image.new('P', (1, 1))
        palette.putpalette([v for c in COLORS for v in c] + [0] * (768-18))
        image = image.quantize(palette=palette, dither=Image.Dither.FLOYDSTEINBERG).convert('RGB')
        used = {color for _, color in image.getcolors(size[0]*size[1])}
        if not used.issubset(set(COLORS)):
            raise ValueError('Unexpected color after quantization')
        target.parent.mkdir(parents=True, exist_ok=True)
        image.save(target, format='BMP')
        if preview:
            preview.parent.mkdir(parents=True, exist_ok=True)
            image.save(preview, format='PNG')
    data = target.read_bytes()
    if (data[:2] != b'BM' or struct.unpack_from('<I', data, 14)[0] != 40 or
        struct.unpack_from('<ii', data, 18) != size or
        struct.unpack_from('<HHI', data, 26) != (1, 24, 0)):
        raise ValueError('BMP header does not match firmware contract')
    stride = (size[0]*3+3) & ~3
    if len(data) != 54 + stride*size[1]:
        raise ValueError('BMP size does not match complete pixel array')
    return {'source': source.name, 'file': target.name, 'width': size[0], 'height': size[1],
            'bpp': 24, 'compression': 'BI_RGB', 'colors_used': len(used),
            'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--preview-dir', type=Path)
    parser.add_argument('--rotation', type=int, choices=[0, 90, 180, 270], default=0)
    args = parser.parse_args()
    size = (480, 800) if args.rotation in (90, 270) else (800, 480)
    sources = sorted(p for p in args.input.iterdir() if p.suffix.lower() in {'.png', '.jpg', '.jpeg', '.webp', '.bmp'})
    if not sources:
        parser.error('No input images found')
    report = []
    for source in sources:
        preview = args.preview_dir / (source.stem+'.png') if args.preview_dir else None
        result = prepare(source, args.output / (source.stem+'.bmp'), size, preview)
        report.append(result)
        print(f"Prepared {result['file']}: {result['bytes']} bytes, {result['colors_used']} colors")
    (args.output / 'conversion.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')


if __name__ == '__main__':
    main()
