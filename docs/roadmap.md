# Status and roadmap

English | [简体中文](zh-CN/roadmap.md) · [Documentation](README.md)

Source version **0.18.0**. Implementation and hardware verification are distinct; see [validation](validation.md).

## Implemented

| Area | Features |
|---|---|
| Hardware | Native Windows IDF, PMIC, SD, RTC, SHTC3, display and buttons |
| Offline album | Validated six-color BMP, sequential/shuffle, two strategies, recovery/deep sleep, saved-network offline playback |
| Network/time | NTP intervals/retries, RTC UTC, manual sync, SD-saved Wi-Fi/sync settings |
| Web maintenance | Status, folders, create/rename/confirmed delete with recovery, photo APIs, previews and selected-photo display |
| Setup/power | Open AP/QR/captive setup, external-power online, battery sleep |
| Speech | Hourly/half-hourly Xiaole Mandarin, environment readings, tests/volume, multiple pauses and NVS deduplication |
| Imports | JPEG/PNG/24-bit BMP, manual composition/frame placement, save-as naming, six-color background conversion/progress |
| Library replacement | Qwen Image 2.1 generation, backup, 100-file network upload/readback |
| Languages | English/Chinese header switch, embedded catalog and bilingual documentation |

## Pending

Manual physical speech/mute interaction needs coordination with KEY provisioning. Microphone activity detection needs baseline/hysteresis, self-sound suppression and power evaluation; it stays disabled. Automatic low-battery policies need actual capacity/current/runtime measurements, thresholds and safe-stop ordering. English speech is not implemented; Xiaole remains Mandarin.

## Further hardware acceptance

1. Long playback across battery/external power and unstable networks.
2. Deep-sleep current, actual battery capacity and runtime.
3. Full-card/hot-removal, malformed configuration, refresh power loss, NVS failures and BUSY timeouts.
4. RTC all-power removal, long-term drift and alarm IRQ wiring.
5. Real 06:00 triggers, cross-day/mute boundaries, sensor failures, refresh crossing speech times and backwards clock adjustments.
6. Physical phone composition/batch use and remaining physical-frame/firmware-rotation combinations.

Short CPU/heap/network-fault tests are documented in [performance](performance-stress.md). Historical M0–M4 context is in [M1](history/m1-implementation.md) and [version records](history/validation-log.md).
