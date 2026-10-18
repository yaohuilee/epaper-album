# ESP-TTS announcement design

English | [简体中文](../zh-CN/design/clock-announcement.md) · [Operation](../announcement.md) · [Build](../esp-tts.md)

Since 0.16.0, device-local ESP-TTS Xiaole, fixed speed 2, handles automatic announcements and tests through the same scheduler/synthesizer/output chain. The old recorded-pack design is [archived](../history/recorded-announcement-design.md). UI language does not select a voice; speech is Mandarin.

## Data and responsibilities

SD global settings → quiet periods and minute 00/30 targets → external RTC and fresh SHTC3 readings → complete UTF-8 phrases → ESP-TTS 16 kHz PCM → 24 kHz PCM/beeps/silences → ES8311/speaker.

The core generates text, merges intervals and computes targets/sample positions. Services validate RTC, schedule/migrate/time-limit, arbitrate resources, deduplicate in NVS and expose HTTP. Devices provide I2S, DAC and amplifier. Flash voice data contains phonetic/dictionary resources, not prerecorded clock announcements.

Four phrase types: “现在时刻”, “X点整/三十分”, temperature in Celsius and relative humidity percent. Temperature/humidity have a 300 ms separation. Final beep→prefix, prefix→time and time→environment each have one second of silence, measured in 24 kHz samples after the preceding sound ends.

## Timing and resources

Five hourly beeps start at T−4; three half-hourly at T−2. The final beep **starts at T**. Preparation begins T−20; battery's last planned wake is T−30. External RTC is checked periodically (15-minute distant, one-minute near-target) and restored at each boot rather than accumulating internal deep-sleep drift.

Each phrase can occupy at most 15 seconds, synthesize within 5 seconds, and overall preparation within 10 seconds. Full PCM is limited to 40 seconds/about 1.83 MiB. Large buffers use PSRAM; DMA/conversion buffers use internal RAM. Voice data is 2,938,039 bytes at `0x600000` in a 4 MiB partition, without moving original state NVS. Release mappings/synthesizer after output.

Shared resource locks prevent concurrent large transfers/photo work from consuming synthesis memory. An already active long task can cause the event to be skipped; overdue speech is not replayed. Persist automatic deduplication before output. Tests do not write that target record.

## Settings and failure paths

ESP-TTS/Xiaole is fixed, with no engine selector or recorded-pack API. Volume, hourly/half-hourly activation and quiet periods save separately. Legacy fields migrate atomically, preserving unrelated global objects. Failed migration does not silently install a partial announcement configuration.

Pause rules use T, inclusive start/exclusive end. A 06:00 resumption permits 05:59:56 beeps. RTC jumps/failure, missing voice data, synthesis failure and timeout have explicit states. Environment synthesis may degrade to time-only; failed time phrases prevent output.

Host tests cover settings, allocation, boundaries, resampling, text/sample positions and cleanup. Hardware acceptance covers both test types, migration, Web and amplifier while retaining album data. Real target triggers, long battery operation and absolute onset accuracy remain separate from immediate test playback.
