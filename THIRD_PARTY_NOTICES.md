# Third-party components and resources

English | [简体中文](THIRD_PARTY_NOTICES.zh-CN.md)

Original project code and documentation use [MIT](LICENSE). The following retain their upstream terms and are not relicensed by project MIT.

| Resource | Use | License |
|---|---|---|
| Project Nayuki QR generator | `components/services/qrcodegen.c/.h`; notices retained | [MIT](third_party/licenses/Nayuki-QR-MIT.txt) |
| Noto Sans SC | Fixed provisioning-label bitmaps; original font stays local | [OFL-1.1](third_party/licenses/NotoSansSC-OFL.txt) |
| Noto Sans Mono | ASCII provisioning glyphs; original font stays local | [OFL-1.1](third_party/licenses/NotoSansMono-OFL.txt) |
| ES8311 initialization parameters | `components/devices/audio_output.c`, with source/copyright attribution to [esp-bsp](https://github.com/espressif/esp-bsp/blob/master/components/es8311/es8311.c) | [Apache-2.0](third_party/licenses/ESP-IDF-Apache-2.0.txt) |
| ESP-TTS / ESP-SR | Installer downloads S3 library, API headers and Xiaole data; no binaries committed | [ESPRESSIF MIT](third_party/licenses/ESP-TTS-Espressif-MIT.txt), restricted to Espressif products; API Apache notices retained |
| ESP-IDF v5.5.3 | External SDK, not copied into repository | [Apache-2.0 main license](third_party/licenses/ESP-IDF-Apache-2.0.txt); bundled components have their own terms |
| esp_jpeg 1.3.1 / TJpgDec | Pinned external software decoder | esp_jpeg [Apache-2.0](third_party/licenses/ESP-IDF-Apache-2.0.txt), TJpgDec [ChaN notice](third_party/licenses/TJpgDec.txt) |
| libpng 1.6.58 / zlib 1.3.2 | Component Manager dependencies pinned in lockfile | [libpng](third_party/licenses/libpng-LICENSE.txt), [zlib](third_party/licenses/zlib-LICENSE.txt); Espressif PSRAM wrapper Apache notices retained |
| Pillow | External desktop image/font tool | [MIT-CMU](https://github.com/python-pillow/Pillow/blob/11.2.1/LICENSE) |

Sources: [Nayuki](https://www.nayuki.io/page/qr-code-generator-library), [Noto Sans SC](https://github.com/google/fonts/tree/main/ofl/notosanssc), [Noto Sans Mono](https://github.com/google/fonts/tree/main/ofl/notosansmono).

## Derived provisioning fonts

`components/services/provision_font.h` contains **AlbumProvisionGlyphs**, a bitmap derivative retaining OFL-1.1 and copyright. Original TTF files are not distributed here; builds use the committed header.

To regenerate, install Pillow and place the two OFL fonts at `.local/fonts/NotoSansSC.ttf` and `.local/fonts/NotoSansMono.ttf`, then run `python tools/provision_font.py`. Options `--cjk-font` and `--ascii-font` allow other paths. The generator is MIT; output font data remains OFL. Retain notices and do not use reserved upstream font names for a derived font.

## SDK, voice and generated media

ESP-IDF components such as cJSON/lwIP retain their file-level licenses. Firmware redistribution must carry the appropriate upstream licenses/NOTICE requirements. Private-credential builds are not public release artifacts.

Since 0.16.0, the fixed official ESP-TTS/Xiaole source tree supplies local libraries, voice data and headers. The ESPRESSIF MIT condition restricts use to Espressif products; this ESP32-S3 meets that scope. The installer retains full LICENSE and API header notices. The repository's MIT does not remove that condition. Local SDK binaries and voice data are ignored. Historical Qwen recording tools are no longer the active speech system or a grant to distribute their outputs.

JPEG uses [esp_jpeg 1.3.1](https://components.espressif.com/components/espressif/esp_jpeg/versions/1.3.1/readme), software TJpgDec with ROM disabled for grayscale support. Preserve both Apache and ChaN notices. PNG uses [libpng 1.6.58~2](https://components.espressif.com/components/espressif/libpng/versions/1.6.58~2/readme) and transitive zlib 1.3.2~1. Sources stay in ignored `managed_components/`; license texts are committed. Original decoding adapters/sampling code use project MIT.

Generation workflows do not include model weights. Models, generated assets and third-party services follow their respective terms. English/Chinese documentation does not replace or translate the authoritative upstream legal license texts.
