# Validation summary

English | [简体中文](zh-CN/validation.md) · [Documentation](README.md)

Results are version-specific, not proof of every later hardware scenario. [Historical records](history/validation-log.md) and the linked Chinese editions retain detailed original chronology.

## 0.18.0 — English and Simplified Chinese

The UI catalog covers static/dynamic messages, dialog labels, accessibility text, import progress and setup. Language preference is browser-local and does not write device settings. Host checks cover browser/explicit/stored locale selection, blocked storage, switching guards, user-name preservation, catalog coverage, compact durations and English execution of all twenty import scenarios. 811 language/catalog/accessibility/duration checks and all twenty English import scenarios passed, alongside the existing C/JavaScript regressions. Native Windows build, COM7 application flash/hash verification and byte-for-byte readback of all eight web assets passed. Browser checks covered all six workspaces, Wi-Fi scanning/dialog labels, interval draft/undo, local composition with duplicate-name suggestion and cancellation, setup, and language switching/navigation. Chinese album/file names remained unchanged. A 390×844 Chrome viewport showed no horizontal page overflow and the drawer started exactly at the wrapped header bottom; the interval dialog and footer fit within the viewport. Two existing photos, the current photo and 86400-second interval remained; no photo was uploaded or setting saved by the UI checks. Cold flashing/reboot naturally started a new full interval.

Bilingual provisioning frame bounds/palette passed and both rendered QR payloads decoded correctly. New bilingual physical-screen appearance, real phone captive setup, audio and battery sleep were not reaccepted in this UI-focused run. GitHub now uses English README/guides/policies and About text, with 37 paired English/Chinese Markdown documents and bilingual issue/PR templates. Local links and counterpart checks passed. Legal notices remain authoritative upstream text; English UI does not add English TTS or a selectable time zone.

## 0.17.7 — Page save-as (2026-10-18)

621 composition/naming checks and twenty import scenarios passed: Unicode/byte limits, reserved/existing/batch names, BMP filename forwarding, late races, partial cancellation, SD failure, explicit busy and uncertain network. Busy has four sends maximum; unknown response one. Folder/display regressions passed. Windows build, COM7 Flash hash and device/local assets matched.

The real browser suggested `_2`, blocked the old name, accepted a Chinese save name and preserved drafts on review. Desktop/390×844 layouts showed controls/footer without horizontal overflow. A real save converted in **11.121 s**; complete readback was 800×480 six-color BMP. Original before/after SHA-256 matched. Count changed one→two; current photo, selected albums and **86400-second** interval matched this run's baseline. Weak-link readback initially interrupted; existing read-only retries completed with browser polling paused. Late concurrent-name races were simulated, not manufactured by a second physical client.

## 0.17.4–0.17.6 — Composition, physical placement and errors

0.17.4 preserved a 1279×1706 no-EXIF portrait with fit/white margins instead of landscape center cropping. 440 geometry/BMP/navigation checks passed. Real conversion **11.106 s**, full BMP readback/six-color white margins and actual device refresh succeeded; prior cropped photo remained.

0.17.5 added independent physical placement; 591 checks covered all sixteen physical/driver rotation combinations with asymmetric pixel reconstruction. Clockwise portrait conversion **11.136 s** and 800×480 readback reconstructed upright 480×800 with white margins. Other combinations were host-tested; the user had not physically rotated/confirmed all of them.

0.17.6 added paged duplicate preflight and distinct SD-path errors. Fourteen scenarios passed. Real `409 exists` started no job and preserved original file hash. Its preflight blocking was superseded by editable save-as in 0.17.7. Parallel downloads once blocked status requests in the single-request HTTP server; completion/reload recovered it.

## 0.17.3 — Display a selected photo (2026-10-16)

Album 1414, runtime 1689, image 518650, announcement 24304, resampling 20 checks and UI regression passed. Enabled/disabled album display refreshed in **33.219/33.109 s**. Duplicate current-content requests did not refresh/reset deadlines; missing-file requests did not change position. Scan/restart preserved manual selection and next-photo returned to the proper enabled successor. Original fifty enabled images and playback settings were restored. No new user visual confirmation or real SD/NVS fault injection in that run.

## 0.17.2 — PNG (2026-10-16)

Twelve valid PNGs converted and read back, covering RGB/RGBA/index transparency, 1/2/4-bit grayscale, 16-bit channels and RGB/RGBA Adam7. Standard/interlaced and precomposited-white equivalents had identical hashes. Four CRC/truncated/APNG/oversized failures were rejected without publication. For 960×576 sources: RGB **10.934 s**, Adam7 RGB **14.717 s**, RGBA **10.562 s**, Adam7 RGBA **15.495 s**, indexed transparent **9.202 s**.

Concurrent Adam7 RGBA/speech/next-photo finished in **68.201 s**, with **52.910 s** waiting. Minimum free PSRAM 4854652 B/internal 104967 B; PSRAM returned to 8110720 B. No reset/OOM observed. Not a 40 MP maximum-file, real power-loss/hot-removal or physical colorimetry test.

## 0.17.0–0.17.1 — Conversion and 8 MiB limit

Standard JPEG/24-bit BMP converted in a low-priority background task and retained playback/speech arbitration. A 4032×3024 JPEG measured **19.414 s** without contention and **72.913 s** with speech/refresh, including **53.495 s** pause. EXIF orientation, grayscale, malformed sources and resource cleanup were checked; see [stress](performance-stress.md).

0.17.1 changed the single source limit to 8 MiB in page and API. Boundary tests covered one byte below/exact/above; oversize rejection occurred before file reception/conversion. The unchanged final-BMP endpoint remains 2 MiB. Subsequent PNG support retained the limit.

## 0.16.0–0.16.3 — Speech, stress, offline playback and folders

Xiaole complete-phrase hourly/half-hourly tests prepared in **4213/4342 ms** and the user confirmed complete audio/pauses. CPU and allocator low-watermark tests, transfer/refresh concurrency, NTP timeout and injected Wi-Fi recovery passed within documented bounds. See [measurements](performance-stress.md).

Saved-network failures and offline cold boots were changed to retain local playback, without automatic setup takeover. Long disconnected operation and later network recovery were tested. Folder create/Unicode rename/confirmed photo+empty-folder delete, stable IDs/deadlines, rollback and interrupted-rename recovery were checked with host/device scenarios. This does not imply physical FAT power-failure atomicity.

## 0.15.0–0.15.3 — Announcement fault recovery and voice trial

Host tests covered target/mute boundaries, allocation/synthesis failures, cleanup, NVS deduplication, incomplete environment fallback and UI actual-completion states. Battery wake testing reported `deep_wake=true`, RTC/system difference −1 second, matching request/completion IDs and audible test speech; the 21:00 deduplication record survived.

User confirmed five/three beeps and complete speech; tone quality was deferred in favor of logic correctness. Xiaoxin Lite/Xiaole/full Xiaoxin were compared and Xiaole selected. Test playback is not a measurement of real target onset, acoustic quality, sound pressure or long-term power.

## 0.14.36–0.14.37 — Night pause

Simulated production clocks covered overnight/same-day/start/end/equal-invalid times, deep wake, manual changes and independent/repeated saves preserving deadlines. Browser save/undo and SD persistence passed. A temporary real pause covering current time kept the photo and extended next-change scheduling. The UI moved to device-time wording and 15-minute steps, retaining old minute values. No overnight wait to real 06:00 or renewed battery-boundary test was claimed.

## 0.14.35 and public-source recheck

The complete generation/replacement flow produced 100 distinct 800×480 six-color BMPs, each **1,152,054 B**, after backing up the old six-album/60-photo library. API replacement made five albums of twenty, then uploaded/downloaded every file for identical SHA-256. All albums enabled at 900 seconds. Next-photo and HTTP worked. Real truncated downloads and receive timeouts motivated bounded safe retries.

Public-source recheck on 2026-10-12 built/flashed without erasing SD/state, confirmed Wi-Fi/HTTP/NTP/RTC/SHTC3/USB host, listed all 100 files, refreshed a photo, and tested setup/success recovery. User confirmed corrected mixed-script baseline alignment and phone connection; the 30-second AP shutdown restored home HTTP. This run did not repeat all power/audio/file tests or visually inspect 100 panel images.

## Scope still outstanding

Deep-sleep current, true battery capacity/runtime, automatic low-battery thresholds, long playback/stress, physical SD/NVS failures, all-power RTC retention/alarm wiring/drift, calibrated audio onset/pressure and activity detection remain separate work. Browser layouts are not a guarantee for every phone/captive browser. Historical references to “current photo/version” refer to that run, not today's settings.
