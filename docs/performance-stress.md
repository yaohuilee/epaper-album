# CPU, memory and concurrency stress tests

English | [简体中文](zh-CN/performance-stress.md) · [Documentation](README.md)

Measured 2026-10-14 on 0.16.1, dual-core 240 MHz ESP32-S3 with 8 MiB PSRAM, Xiaole speed 2/volume 40%. FreeRTOS ESP Timer runtime counters sampled every 100 ms and at phase boundaries. **100% means both cores together**, not one core. Tasks may migrate.

| Phase | Average total CPU | Notes |
|---|---:|---|
| Synthesis/preparation | 38.25% | Announcement task 35.81%, about 71.6% of one core |
| Playback | 2.28% | Announcement task 0.59% |
| Transfers/file operations | 21.13% | HTTP 14.76%, TCP/IP 3.00%, Wi-Fi 2.17% |
| E-paper refresh | 2.58% | Main 0.92%; primarily waits for panel |
| NTP request/timeout | 1.78% | Primarily network waiting |
| Idle/control | 2.49% | Includes polling and cleanup |

Profiler cost around 0.7% is included. These are phase means, not instantaneous peaks; overlapping phases must not be summed.

| Heap | Initially free | Window minimum free | Incremental use |
|---|---:|---:|---:|
| Internal | 145207 B | 88959 B (86.9 KiB) | 56248 B |
| PSRAM | 8111104 B | 7236432 B (6.90 MiB) | 874672 B |

Allocator low-water marks, rather than sampling alone, capture short allocations. Their per-region sum is conservative and minima need not coincide. Incremental use is not TTS-exclusive memory and excludes preexisting stacks/static/network usage. End values: PSRAM 8111104 B, internal 143895 B, largest internal free block 31744 B.

## Scenarios

Three 1,152,054-byte BMP uploads and four downloads matched SHA-256 in an isolated disabled album. During speech, download returned 409, next-photo accepted 202, duplicate next returned 409, then refresh waited until speech ended. NTP at unreachable `192.0.2.1` timed out without invalidating RTC; conflicting tests/refresh were rejected, and restoring servers restored sync.

Injected Wi-Fi loss after 1 second attempted reconnection after 15 seconds via an independent recovery task. Interrupted uploads were not published; Web and speech recovered. Another outage during synthesis/playback still completed speech, with preparation 3714 ms. Cleanup initially saw stale cached busy state; the tool was changed to poll live file availability and retry only explicit rejection.

The original five albums/100 photos, NTP servers, quiet periods and announcement deduplication state were retained. Test albums were removed. No deadlock, reset or out-of-memory was observed. Real 06:00 trigger, long-running stress, low battery and absolute audio timing were not tested.

## Reproduce

Enable `CONFIG_EPAPER_DIAGNOSTICS` (normally off), build/flash and keep computer USB attached:

```powershell
python -m pip install requests
python tools/stress_device.py --url http://DEVICE_IP --output ../stress-results
```

The tool creates/cleans a disabled test album, temporarily changes NTP and restores it in finally. Keep output outside the repository; no Wi-Fi password/token is recorded. Protected endpoints: `GET /api/diagnostics`, `POST /api/diagnostics/reset` with `{}` while idle, and `POST /api/diagnostics/network-fault` with `{}` only when a USB host and restoration task are available. Recovery reconnects, waits up to 10 seconds, then returns to normal retry logic. Reset clears injection. Task snapshot capacity is 48; nonzero `snapshot_failures` invalidates unqualified CPU conclusions.

## Image conversion concurrency

0.17.0 (2026-10-15): a 4032×3024 baseline JPEG conversion overlapped an hourly test and queued next-photo. Speech and display succeeded; conversion progressed through waiting to completion in **72.913 s**, including **53.495 s** pause. Uncontended conversion was **19.414 s**. Internal free fell from 156503 to 87319 B; PSRAM 8110720 to 4923208 B. End: internal 152243 B, PSRAM restored to 8110720 B. Snapshot failures: zero. Major JPEG workspace bound about 2.26 MiB; 1200-pixel-wide BMP about 1.17 MiB.

0.17.2 (2026-10-16): Adam7 RGBA PNG with speech/next-photo completed in **68.201 s**, including **52.910 s** pause. Minimum free PSRAM 4854652 B, internal 104967 B; PSRAM restored to 8110720 B. RGB PNG conversion 10.934 s, RGB Adam7 14.717 s, RGBA 10.562 s, RGBA Adam7 15.495 s, indexed transparent 9.202 s for the measured 960×576 sample. These are individual device samples, not maximum-size or universal performance guarantees. See [validation](validation.md).
