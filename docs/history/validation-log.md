# Version validation record

English | [简体中文](../zh-CN/history/validation-log.md) · [Current summary](../validation.md)

This English edition organizes historical results by milestone. The Chinese counterpart preserves original detailed chronological observations. Historical settings/counts belong to that run and do not override current configuration.

| Version / date | Result and boundary |
|---|---|
| 0.2.0 / 2026-10-09 | Native Windows IDF 5.5.3/MSVC build, calendar/CRC, album 843/runtime 55 checks. Initial application 383616 B. 16 MiB backup verified. Separate SD test directories preserved personal files. Real refresh 30.12/30.27 s, BOOT advance, KEY retention and PWR restart without redundant redraw confirmed. Real destructive failures/current/runtime untested. |
| 0.3.0 / 2026-10-10 | Local-only Wi-Fi merge, NTP RTC UTC write/readback, album 854/runtime 58 and two synthetic import tests. Six albums/60 photos retained. Built application 930032 B. Credentials remained local. |
| 0.4–0.14 | Status/control/file APIs, grid/sidebar UI, dialogs, timestamps, memory/power facts, Wi-Fi and time settings, scan/captive setup and QR alignment evolved incrementally. These are version-specific UI/protocol checks, not blanket compatibility certification. |
| 0.14.34 | Charger without USB host remained online; battery removal/restoration checks observed offline and recovered Wi-Fi/Web. No current measurement or raw reset-reason proof from network-only observations. |
| 0.14.35 | Generated 100 world landscapes, backed up prior 60 photos, created five albums, uploaded/read-verified all 100 SHA-256, enabled 900 s playback; handled real incomplete transfers with safe retries. |
| 2026-10-12 recheck | Public source build/flash, Wi-Fi/NTP/RTC/environment/status/file counts and next-photo passed. User verified setup alignment and saving; 30 s grace ended and home HTTP/playback returned. |
| 0.14.36–0.14.37 / 2026-10-13 | Night-pause boundaries, independent SD saves, real currently-active pause, device-time wording and 15-minute selection passed. No actual overnight/06:00 battery run. |
| 0.15.0–0.15.2 / 2026-10-13–14 | Recorded speech sequencing/fault paths, actual test completion and wake RTC restoration fixed. Battery test deep_wake=true, RTC difference −1 s, audible output and deduplication preserved. User confirmed basic beep/speech/photo behavior; acoustic refinements deferred. |
| 0.15.3 | Official voice comparison A Xiaoxin Lite/B Xiaole/C Xiaoxin; user selected B. |
| 0.16.0 / 2026-10-14 | Unified Xiaole full-phrase offline synthesis; hourly/half preparation 4213/4342 ms, audible complete tests confirmed; recorded-pack APIs retired. |
| 0.16.1 | CPU/heap, transfers, queued refresh, NTP timeout and independent Wi-Fi fault recovery passed. Synthesis total CPU 38.25%; window minimum internal 88959 B/PSRAM 7236432 B. Not absolute onset/current/long-duration certification. |
| 0.16.2 / 2026-10-15 | Saved-network long outage/offline boot retains playback and does not automatically provision; recovery restores network service. |
| 0.16.3 | Album create/rename/delete and interrupted rename handling, stable IDs/state and Unicode folders validated; physical SD power-loss limits remain. |
| 0.17.0 | JPEG/BMP background imports and concurrency; large JPEG 19.414 s uncontended, 72.913 s with tasks/53.495 s pause. User later confirmed JPEG/BMP actual screen results and original playback restored. |
| 0.17.1 / 2026-10-16 | 8 MiB input boundary synchronized across API/page/tests/docs; below/exact/above verified. |
| 0.17.2 | Twelve PNG variants converted/read back; equivalent alpha/interlace/grayscale hashes matched; malformed/APNG/oversized failed without publication. Concurrent speech/refresh completed. |
| 0.17.3 | Selected enabled/disabled photo display, duplicate/no-refresh, scan/restart and successor behavior passed; 33.219/33.109 s refresh, baseline playback restored. |
| 0.17.4 / 2026-10-18 | Local review/fit/crop fixed portrait content loss. 440 editor checks, real 11.106 s conversion and six-color full-content readback. |
| 0.17.5 | Physical placement independent of source/driver rotation; 591 checks for sixteen combinations; real clockwise portrait 11.136 s and pixel reconstruction. |
| 0.17.6 | Fourteen duplicate/error UI scenarios; real 409 without job and unchanged original bytes. |
| 0.17.7 | 621 editor/twenty import checks, real Chinese save-as 11.121 s; original/current/options retained; complete six-color output readback. |
| 0.18.0 | 811 language checks plus twenty English import scenarios, existing C/UI regressions, Windows build and COM7 flash passed. Eight embedded assets matched source. Six workspaces, local composition/cancel, interval draft/undo, setup and 390×844 layout passed. Rendered QR payloads decoded; no new physical bilingual screen/phone/audio/battery acceptance. English About and 37 documentation pairs prepared. |

Detailed measurements and limits: [validation summary](../validation.md), [stress](../performance-stress.md), [composition](photo-import-composition.md). Unverified physical power faults, battery current/capacity, RTC alarm wiring and long-term timing remain explicitly outstanding.
