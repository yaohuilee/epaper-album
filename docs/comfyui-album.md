# Early ComfyUI 60-photo workflow

English | [简体中文](zh-CN/comfyui-album.md) · [Documentation](README.md)

> Historical production example. This library was backed up and replaced by the later [100-photo world workflow](world-album-workflow.md).

On 2026-10-09, local ComfyUI generated 60 images and copied converted files to SD. Source workflows/manifests remain in `examples/comfyui/`; images, generated packages and raw logs are not committed.

| Group | Count |
|---|---:|
| Landscapes | 4 |
| Science-fiction scenes | 3 |
| Anime scenes | 3 |
| Japanese-style adult anime characters | 20 |
| Chinese qipao, realistic / anime | 10 / 10 |
| Science-fiction characters | 10 |

Sources were 1280×768. DreamShaperXL Turbo generated landscape/realistic qipao/science-fiction images; niji SDXL generated anime. `album-workflows.json` records prompt, seed, local model filename, sampling and complete ComfyUI API node graph. Models must already be available locally; a seed does not guarantee identical pixels across software/hardware versions.

## Reproduce a source

With ComfyUI listening on local port 8188, run at repository root:

```powershell
$album = Get-Content ./examples/comfyui/album-workflows.json -Raw -Encoding utf8 | ConvertFrom-Json
$item = $album.items[0]
$body = @{ prompt = $item.workflow; client_id = [guid]::NewGuid().ToString() } | ConvertTo-Json -Depth 30
$job = Invoke-RestMethod http://127.0.0.1:8188/prompt -Method Post -ContentType 'application/json; charset=utf-8' -Body ([Text.Encoding]::UTF8.GetBytes($body))
$job.prompt_id
```

Read `/history/<prompt_id>`; node 7 reports output names. Sources remain in ComfyUI output grouped by `filename_prefix`. These are API graphs, not frontend canvas-import documents. Convert with `tools/prepare_photos.py`; see [format](image-format.md).

All 60 generated files passed the same C decoder as firmware and SD-copy SHA-256 readback. This established format/copy integrity, not individual physical-screen acceptance.

`examples/comfyui/albums.json` defines six directories, natural sequential album-by-album playback, 900-second dwell and position recovery. Global root/manifest were `/albums` and `albums.json`. Back up existing settings before SD changes, safely eject, reinstall and restart; adding files does not erase the last successful position.

At that M1 stage, timer wake refreshed for about 30 seconds, retained a 10-second awake window and slept. BOOT/KEY also woke it. Panel/amplifier power was reduced but not every rail disabled. Sleep current/runtime were unmeasured. Current external-power behavior differs; see [power](usb-power.md).
