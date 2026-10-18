# Retired recorded-voice design

English | [简体中文](../zh-CN/history/recorded-announcement-design.md) · [Current ESP-TTS design](../design/clock-announcement.md)

> Archived through 0.15.3. The recorded-pack mechanism and its HTTP installation endpoints are no longer active.

The original design generated female-voice recordings on a computer, normalized/converted clips and assembled fixed phrases/numbers at runtime from SD. It covered time prefixes, hours, half-hour endings, temperature, humidity and unavailable messages, with a manifest for format/identity checks. A later trial offered ESP-TTS alongside this pack before unified Xiaole synthesis replaced it.

The accepted timing was five hourly/three half-hourly beeps, final beep at the target, then one-second gaps before prefix, time and environmental readings. Volume tests and independent quiet periods were part of the design. E-paper could not use transient LCD-style notices. Failures needed cleanup, bounded preparation and no catch-up/repeated target output.

User voice feedback requested brighter, fuller speech and a slower pace. Those recording trials are historical evidence, not current audio assets or a license to redistribute generated speech. Since 0.16.0 complete short phrases are synthesized on the device by Xiaole. Old configuration fields migrate; old SD recordings remain unused, not automatically deleted. See [third-party notices](../../THIRD_PARTY_NOTICES.md) and [announcements](../announcement.md).
