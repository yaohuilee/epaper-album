# Photo format

English | [简体中文](zh-CN/image-format.md) · [Documentation](README.md)

The library accepts ordinary JPEG, PNG and uncompressed 24-bit BMP sources. Files used for playback on SD remain target-size six-color BMPs.

## Physical orientation and composition

Landscape composition uses 800×480; clockwise/counterclockwise portrait uses 480×800. Upside-down landscape is supported. Physical frame orientation is independent of photo rotation. A portrait source does not establish how the frame is physically placed.

The browser applies decoded EXIF orientation, then user rotation, and draws a white-backed canvas. The default **Fit whole photo** preserves the whole image with margins. **Fill and crop** uses the display aspect ratio, 100–400% zoom and draggable position. Confirm the entire batch before any transfer.

Before upload, pixels are rotated by `−(F+R)`, where F is physical frame rotation and R is firmware decoding rotation. This preserves the existing 800×480/480×800 input contract. No extra orientation metadata is stored. Raw BMP previews on a computer or in the library may look sideways because they show panel data. Existing photos are not reoriented automatically; lost cropped content requires the original source.

The browser encodes a positive-height 24-bit BMP with the reviewed save name. The device still performs brightness adjustment and dithering. Composition recipes are not saved on SD. To recompose later, select the source again and save under a new name. The source and existing device photo remain intact.

## Direct source import API

`PUT /api/upload` accepts 54 bytes through **8 MiB (8,388,608 bytes inclusive)**, maximum 12,000 pixels per side and 40 megapixels total.

| Format | Supported | Rejected |
|---|---|---|
| JPEG | Baseline RGB/YCbCr or grayscale; eight EXIF orientations | Progressive, CMYK, damaged data |
| PNG | RGB, RGBA, grayscale, grayscale alpha, indexed; standard 1/2/4/8/16-bit combinations; Adam7 | APNG, invalid CRC, truncation |
| BMP | 24-bit BI_RGB; 40–124-byte DIB; positive or negative height | Indexed, 32-bit, compressed |

PNG transparency is composited on white; 16-bit channels use their upper 8 bits. Direct source uploads are center-cropped and bilinearly resized according to device rotation. Web uploads first compose the image in the browser, so this default crop does not choose the content.

Automatic brightness stretching and slight contrast adjustment precede serpentine Floyd–Steinberg six-color error diffusion. Direct API imports keep the source stem with `.bmp`; the web editor can choose another stem and suggests unused names. Outputs never silently replace existing BMPs.

The low-priority conversion task yields to announcements and display refresh, then resumes. SD temporary source/output files are removed after success/failure and cleaned after interrupted boots. The original computer file is not modified. No APNG, WebP, HEIC or ICC profile conversion is implemented in firmware. Convert special color spaces to sRGB beforehand. PNG ancillary color/text metadata does not drive rendering. File palette values are not measured panel reflectance colors.

## Playback BMP and unchanged upload

Direct SD copying and `PUT /api/photo` require final files; this endpoint does not convert.

- 800×480 for rotation 0/180; 480×800 for 90/270.
- Windows BMP with 40-byte BITMAPINFOHEADER, planes=1, 24 bpp, BI_RGB=0.
- Positive height, bottom-up rows. Indexed, negative-height playback, 32-bit alpha, RLE and bitmask compression are rejected.
- BGR pixels; each row padded to four bytes. Header offset, bounds, dimensions, stride and length are checked.
- Only RGB `000000`, `FFFFFF`, `FF0000`, `FFFF00`, `0000FF`, `00FF00` are allowed: black, white, red, yellow, blue and green.
- Panel codes are respectively 0, 1, 3, 2, 5, 6. Two pixels per byte; an 800×480 frame uses 192,000 bytes.
- A conventional 54-byte header produces a 1,152,054-byte file. Size alone does not establish validity.

Apply orientation, sRGB and transparency first; fit/crop and resize before quantization. Do not resize or JPEG-compress after dithering. Text and lines should retain pure palette colors. Photos display without temporary clock/status overlays; setup has a separate screen.

## Desktop tools

```powershell
python -m pip install Pillow
python tools/prepare_photos.py originals prepared --preview-dir previews
```

This tool accepts PNG/JPEG/WebP/BMP, applies EXIF, composites white, center-crops, uses Lanczos resizing, slight contrast/sharpening and fixed-palette dithering, then writes 24-bit BMP without overwriting. Default output is 800×480. Add `--rotation 90` or `270` and use the corresponding device rotation for portrait input. `conversion.json` records size, format and SHA-256. Preview PNGs share the BMP pixels, not the real panel reflectance.

ComfyUI can generate 1280×768 landscape sources at the target aspect ratio. Keep source images and workflows on the computer; only final BMPs belong in playback folders. `tools/validate_bmp.c` with `components/core/bmp.c` uses the actual decoder to validate full images; landscape usage is `validate_bmp photo1.bmp photo2.bmp`.
