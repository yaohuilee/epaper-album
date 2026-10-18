# Sound and announcements

English | [简体中文](zh-CN/announcement.md) · [Design](design/clock-announcement.md) · [ESP-TTS](esp-tts.md)

Since 0.16.0, automatic and test announcements use **ESP-TTS Xiaole**, speed 2, synthesized offline from current time and sensor readings. There is no SD recording pack or Wi-Fi dependency. Speech remains Mandarin in the English UI.

## Sequence

Hourly: 5 beeps from T−4 seconds. Half-hourly: 3 beeps from T−2. Each ordinary beep is 1000 Hz/100 ms; the final beep is 1500 Hz/250 ms and **starts at target time T**.

After the final beep ends, wait 1 second, speak “现在时刻” (the time is now), wait 1 second after that phrase, speak “X点整” or “X点三十分”, wait 1 second after the time, then speak temperature and humidity. Temperature uses up to one decimal, suppressing zero decimals and negative zero; humidity rounds to an integer.

Set hourly/half-hourly activation, volume and up to eight independent daily quiet periods. Same-day and overnight ranges include start and exclude end. Rules evaluate target T; a 22:00–06:00 pause permits 06:00 beeps starting at 05:59:56.

Tests use draft volume without saving and ignore quiet periods. They are unavailable during/immediately before automatic speech. The page waits for actual audio completion and reports partial temperature/humidity failure separately.

## Configuration and migration

Global `announcement` retains `enabled`, `minutes` 0/30, `volume_percent`, temperature/humidity inclusion, `temperature_decimals` 0/1 and `pause_periods`. `engine=esp-tts`, `voice=xiaole`, `speech_speed=2` are fixed.

Legacy `voice_pack`, `audio_root` and `quiet_hours` migrate atomically with the existing SD/cache mechanism. Wi-Fi, albums, volume and quiet periods are retained. A failed write disables announcements and reports a configuration error. Old SD audio files remain but are unused.

## Scheduling and failures

External RTC restores the clock, storing UTC. Battery scheduling wakes by 30 seconds before target and prepares at T−20: validate RTC, sample the sensor, synthesize the full PCM sequence, then anchor final beep to the RTC second transition. Each phrase has a 5-second synthesis limit, overall preparation 10 seconds and audio sequence 40 seconds. Late preparation is not played as an overdue announcement.

Prepare in RAM, resample 16 kHz to 24 kHz, then play through ES8311. Release buffers, synthesizer and Flash mappings afterward. During preparation/playback, refresh and long tasks wait; next-photo requests merge into a queue. E-paper does not display transient speech notices.

Sensor failure speaks an unavailable message. Environment synthesis may degrade to time-only speech; time synthesis failure cancels the event. Missing/invalid Xiaole Flash data prevents automatic activation. Before automatic audio, persist a deduplication record to avoid repetition after reset. Missed announcements are not caught up.

## API

All writes require `X-Album-Token`:

| Route | Purpose |
|---|---|
| `POST /api/announcement-settings` | Save enabled minutes and volume |
| `POST /api/announcement-pause-settings` | Save independent quiet periods |
| `POST /api/announcement-test` | `kind=hour/half`, volume 1–100; optional engine accepts only `esp-tts` |

State includes engine, voice, speed, readiness, phase, target, last result, request/completion IDs and synthesis duration. A successful control job means queued, not audio completed. Use `completed_test_id/last_test_result`. Recorded-pack upload/download/activation endpoints have been removed. Absolute onset error, calibrated sound pressure and long-term power need instruments.
