# M1 implementation baseline

English | [简体中文](../zh-CN/history/m1-implementation.md) · [Current architecture](../architecture.md)

> Historical 0.2.0 baseline, not current feature limits.

M1 established native Windows ESP-IDF v5.5.3, ESP32-S3, 16 MB Flash, 8 MB Octal PSRAM and six-color 800×480 playback. PMIC, SD, RTC, SHTC3, display and buttons were initialized independently of vendor firmware. The two configuration files separated global settings from album manifests; NVS retained runtime position/caches.

Playback already validated final BMPs, naturally sorted files, supported sequential/shuffle, two album strategies and restart recovery. Dwell starts after refresh completes. Failed refresh does not advance position. Normal deep-sleep wake preserves deadlines; cold boot starts a fresh full interval without catch-up. Empty/bad albums retain the screen and stop automatic updates.

## Installation and data

Initial migration verified target MAC and backed up/read-verified the full 16 MiB Flash. The new `album_state` partition at `0x500000`, size `0x20000`, was initialized once; routine updates preserve it. Do not reuse this initial-erase procedure for upgrades.

Independent SD test directories preserved personal photos. On-device refresh was about 30 seconds, followed by a 10-second awake window then sleep. BOOT advanced; KEY retained the photo. PWR shutdown/restart retained the second image without another refresh, confirmed by the user.

At M1, Wi-Fi, Web management, audio announcements, source conversion, activity detection and battery policy were not yet present. Subsequent versions implemented many of these; [roadmap](../roadmap.md) is authoritative now.

Host tests covered calendar/CRC, configuration, BMP and queue behavior plus mocked production runtime. Real SD/power-loss/NVS/timeout failures, sleep current, battery runtime and long operation were not comprehensively verified. Original firmware, dumps, photos and probe logs stay outside the repository.
