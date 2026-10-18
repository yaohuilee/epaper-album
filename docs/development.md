# Native Windows development

English | [简体中文](zh-CN/development.md) · [Documentation](README.md)

Use ESP-IDF **v5.5.3**, ESP32-S3, 16 MB Quad Flash and 8 MB Octal PSRAM. No WSL/Docker is required. Prefer short SDK/project paths without spaces.

## Install and build

Use the [official Windows instructions](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32s3/get-started/windows-setup.html), or an existing native Git/Python environment:

```powershell
git clone --branch v5.5.3 --depth 1 --recursive --shallow-submodules https://github.com/espressif/esp-idf.git C:/Espressif/esp-idf-v5.5.3
$env:IDF_TOOLS_PATH = 'C:/Espressif/tools'
python C:/Espressif/esp-idf-v5.5.3/tools/idf_tools.py install --targets esp32s3
python C:/Espressif/esp-idf-v5.5.3/tools/idf_tools.py install-python-env
```

Paths may differ; no permanent system PATH edits are required. For a stale Git proxy, a command-local `git -c http.proxy= clone ...` avoids overwriting global networking settings.

Create ignored `.local/idf-env.json` at the project root with actual paths:

```json
{"idf_path":"C:/Espressif/esp-idf-v5.5.3","tools_path":"C:/Espressif/tools","python_directory":"C:/Python312"}
```

```powershell
. ./tools/Enter-Idf.ps1
./tools/Install-EspTts.ps1
./tools/Build.ps1
```

Alternatively pass `-IdfPath`, `-ToolsPath`, `-PythonDirectory`. Activation uses official `export.ps1`; repeated activation retains the active IDF Python environment. Build compiles only and does not flash.

## Tests

`./tools/Test.ps1` locates Visual Studio C++ Build Tools and compiles host programs with SDK cJSON. Override `-VsDevCmd` or `-IdfPath` if needed. Tests and synthetic SD files stay under `.local/`.

| Test | Coverage |
|---|---|
| `core_test.c` | Calendar, BCD, leap days, oscillator flags, CRC |
| `album_test.c` | BMP, configuration, catalog, strategies, shuffle/recovery |
| `runtime_test.c` | Production runtime with mocked clock/display/NVS, deadlines and recovery |
| `provision_test.c` | Frame bounds, palette and invalid input |
| `announcement_test.c` | Scheduling boundaries, migration, allocation/synthesis failures, beep/silence sequencing |
| `image_test.c` | Sampling, orientation, dithering, BMP and PNG contributions |
| `pcm_resample_test.c` | 16→24 kHz interpolation, extremes and buffer capacity |
| `*_ui_test.cjs`, `photo_editor_test.cjs` | Actual page scripts, jobs, folder/photo operations, composition and imports |
| `i18n_test.cjs` | Catalog completeness, locale behavior and English/Chinese regression |

Node.js is needed for UI tests. Credential import can be tested independently with `python -X utf8 tests/import_wifi_test.py`, using only synthetic temporary data. Run `python -X utf8 tools/check_docs.py` for bilingual links/counterparts.

`CONFIG_EPAPER_DIAGNOSTICS` is off by default. It enables task CPU/heap statistics and bounded USB-host-only network fault injection; see [stress](performance-stress.md). Host tests/builds do not replace display, button, power and sleep acceptance.

## Flash and upgrade

Before first migration from vendor firmware, verify MAC, back up and verify all 16 MB Flash, review the layout and initialize the project state partition. `album_state` is at `0x500000`, size `0x20000`; see [historical M1](history/m1-implementation.md). An ordinary project upgrade preserves this partition; do not erase all Flash or initialize state again.

After activating IDF, use the actual device port:

```powershell
idf.py -p COM5 flash
```

COM5 is an example, not a guaranteed target. SD data remains separate. Flash includes the Xiaole voice partition at `0x600000`, 4 MiB. No SD recordings are needed. Local provisioning builds may contain private credentials and must not be published.

## Dependencies

The local ESP-TTS installer downloads fixed official libraries/headers/Xiaole data into `.local`; see [ESP-TTS](esp-tts.md). `CONFIG_EPAPER_ESP_TTS` defaults on and requires this dependency.

Component Manager resolves pinned `espressif/esp_jpeg 1.3.1`, `espressif/libpng 1.6.58~2` and `espressif/zlib 1.3.2~1` from the component manifest/lockfile. First build needs network. `managed_components/` is not committed. `CONFIG_JD_USE_ROM=n` uses software TJpgDec for grayscale; ROM mode is rejected at CMake configuration. Image adapters/decoders use `-O2`.

PNG rows and bounded decoder allocations use PSRAM. Adam7 contributions accumulate only updated pixels without a full-resolution RGBA frame. `core/image.c` is host-testable; service tasks handle temporary files, progress and publication.

UI resources and both languages are embedded directly by CMake. No frontend build toolchain or runtime npm dependency is required. Keep additions locale-neutral in protocols and document both languages.
