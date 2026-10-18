# Early announcement and activity planning

English | [简体中文](../zh-CN/design/announcement-and-activity.md) · [Current announcement design](clock-announcement.md)

This records the early plan to add time/environment speech and optional ambient activity detection to the offline album. The implemented speech design is now [ESP-TTS Xiaole](../announcement.md); this plan does not enable microphone activity or low-battery policies.

The initial goals were hourly/half-hourly time, SHTC3 temperature/humidity, independent quiet periods and battery-aware scheduling without transient e-paper notices. Playback must work without a network. Runtime failures should preserve the current image and avoid repeated or overdue speech.

Dual ES7210 microphones could later estimate ambient level/activity, not perform speech recognition. A practical implementation needs a measured baseline, hysteresis, minimum active/quiet dwell, microphone power policy and suppression of the device's own speaker. A quiet state might suppress announcements, but this behavior is undecided and remains disabled in configuration.

Sensor CRC/error handling, reliable RTC, volume control, speaker gain and safe power sequencing are prerequisites. Battery thresholds must come from actual measurements; a charging-state flag is not a remaining-capacity estimate. Current work prioritizes predictable offline scheduling over speculative activity behavior. See [roadmap](../roadmap.md) for pending acceptance.
