# Runtime architecture

English | [简体中文](zh-CN/architecture.md) · [Documentation](README.md)

This describes the current implementation. Activity detection and automatic low-battery policies are not implemented.

| Location | Responsibility |
|---|---|
| `main/app_main.c` | Boot, buttons, task advancement, network lifetime and sleep |
| `components/board/` | GPIO, I2C, display SPI and PMIC initialization |
| `components/core/` | Calendar, CRC, BMP, image sampling/dithering, sorting |
| `components/devices/` | RTC, SHTC3 and audio devices |
| `components/storage/` | SD mount, global settings and unmount |
| `components/album/` | Validation, catalog, playback queue and file rules |
| `components/services/` | Display, NVS/runtime, synchronization, Wi-Fi, setup and HTTP |

## Execution

Boot/wake initializes hardware, mounts SD and validates settings, restores the successful photo/deadlines, then processes buttons, HTTP and due events. External power keeps it online. On battery, completed tasks lead to timer/button deep sleep.

Cold restart begins a full photo interval without catch-up. Normal deep-sleep wake retains deadlines. Only successful display commits a playback position. Failed refresh retains the old position and schedules retry.

SD root settings and album manifest are separate. NVS partition `album_state` holds position, shuffle seed/cursor, dirty-screen state and last-valid configuration. Application tasks perform validated temporary-file/readback/backup saves; HTTP does not directly mutate playback queues. SD transfer, configuration writes, decoding and refresh share a lock. File updates trigger index rebuilds. Neither SD nor NVS is automatically erased as error recovery.

## Network, time and power

HTTP and NTP share Wi-Fi lifetime. USB/external power keeps them online; battery completion stops services before unmount and sleep. External RTC stores UTC; device displays UTC+8. Clock adjustments preserve remaining photo dwell time.

First unconfigured boot or long KEY enters AP+STA provisioning, pauses photo/sync tasks and prevents sleep. Saved-network failure does not automatically provision. After successful setup, a 30-second hotspot grace period precedes restoration of playback. See [setup](wifi-provisioning.md).

Display validates BMP and packs a 192,000-byte frame. Full refresh takes about 30 seconds. Equal pixel content is not refreshed. ESP timer and buttons wake deep sleep; external RTC alarm IRQ wiring remains unconfirmed. PMIC hardware shutdown cannot use the ESP sleep timer. RTC RAM differs from the external clock chip.

## Speech and resource coordination

ESP-TTS Xiaole maps Flash voice data, synthesizes complete Mandarin phrases as 16 kHz PCM and resamples to 24 kHz. A complete beep/silence/speech sequence is prepared before playback, starting preparation 20 seconds before the target. Buffers, synthesizer and mappings are released afterward. No SD recordings or voice-pack API remain.

Announcements do not refresh e-paper. Refresh, synchronization and transfers are deferred/rejected in the protection window; next-photo requests merge into a queued request. Optional diagnostics measure per-phase CPU and heap low watermarks; a separate task restores injected network faults. Normal builds disable diagnostics. See [stress](performance-stress.md).

## Image imports

The single priority-1 `photo_import` task uses a 12 KiB stack. JPEG streams MCUs with an initial decode scale into a target-size accumulator, without a full-resolution RGB allocation. BMP samples source rows. PSRAM holds major buffers; the target buffer is reused for BGR output. PNG uses libpng row decoding to RGBA, white compositing and the same 16-bit sampling workspace. Adam7 contributes only newly updated pixels. Input dimensions, metadata and allocator usage are bounded; CRC/truncation errors use shared cleanup.

Short SD lock regions and per-row resource checks let speech and refresh take priority. Publication and indexing use file revisions. Battery sleep waits for conversion cleanup. HTTP still services one request at a time; source reception may delay status reads. After reception, clients poll conversion progress.

## Selected photo and browser composition

Selected-photo requests fully decode before refresh and NVS commit. Enabled-album selection advances that album's queue. A disabled-album selection uses a manual marker and returns to the original enabled queue after its interval without editing the manifest. Stable IDs preserve queues through scans/renames; failures do not advance positions.

`photo_editor.js` composes local sources on a white target canvas. Batch review stores lightweight recipes; transmission reloads one source at a time, encodes positive-height 24-bit BMP and releases image/canvas data. Direct API source uploads retain center-crop defaults. For physical rotation F and decoder rotation R, the browser rotates by `−(F+R)` using integer-coordinate pixel copies, then the driver/physical rotations reconstruct the preview. No extra metadata is needed.

Recipes include final save names. Album and batch sets provide unique suggestions; `prepare()` uses that name without modifying the source File. Explicit `409 exists` refreshes names and returns unfinished recipes to review. Completed files are removed from selection. A fresh confirmation is required. Explicit busy allows at most four sends; SD errors and uncertain network responses do not trigger rename/retransmit retries.

## Bilingual UI

Embedded `i18n.js` initializes locale and static text before application scripts. Dynamic messages call the same catalog. Language switching reloads the current route, preserving only browser preference, not unsaved drafts. API models and user data remain unchanged. See [language support](language-support.md).
