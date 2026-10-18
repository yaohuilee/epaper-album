# Network replacement with 100 world landscapes

English | [简体中文](zh-CN/world-album-workflow.md) · [Documentation](README.md)

## Generation and format

The tested local ComfyUI workflow used Qwen Image 2.1 UC FP8, Qwen3-VL 8B BF16 text encoder and Qwen Image 2.1 BF16 VAE. Each location had its own prompt/seed: 1280×768 landscape, 25 Euler/simple steps, CFG 1.0. These are **AI-generated landscape artworks**, not documentary travel photographs.

Resize to 800×480, apply light contrast/sharpening, then Floyd–Steinberg quantize to black/white/yellow/red/blue/green. Each uncompressed 24-bit BMP is 1,152,054 bytes. Do not resize afterward. The production decoder validates header, dimensions and every pixel.

Five albums of twenty photos each, enabled at 15-minute intervals:

| ID | Region |
|---|---|
| `world-asia` | Asia |
| `world-europe` | Europe |
| `world-africa` | Africa |
| `world-americas` | Americas |
| `world-oceania-polar` | Oceania and polar regions |

## Workflow

`tools/generate_world_album.py` generates sources/workflows and final BMPs. `tools/replace_album_library.py` backs up the old library, validates all 100 local files, deletes old photos/empty albums, creates disabled target albums, then uploads and downloads every photo for SHA-256 comparison. Only after all checks pass are the new albums enabled. Commands and resume rules are in [file API](http-files.md).

Empty-folder deletion refuses hidden/non-BMP/nested data instead of recursively removing it. Global Wi-Fi/time settings remain. The bulk workflow added API capacity, TCP_NODELAY, active Wi-Fi without modem sleep, 32 KiB file buffers and 8 KiB transfer chunks. Upload response allowance covers SD flush and full pixel validation. Incomplete downloads retry up to four times and never count partial data as success.

Only explicit busy or server-confirmed discarded `408 upload_incomplete` writes may retry within bounds. Existing-name errors and uncertain client timeouts are not blindly resent. Resume validates album/directory identity and downloads existing files before skipping them.

Source images, BMPs, previews, workflows, prior library backups and `installation.json` remain outside the source repository. Credentials/tokens/private photos/vendor firmware are not published. See [validation](validation.md) for actual acceptance limits.
