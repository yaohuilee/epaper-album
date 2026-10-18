# HTTP status and control evolution

English | [简体中文](../zh-CN/history/http-status-changes.md) · [Current API](../http-status.md)

The original read-only status page gained album selection, interval persistence and manual next-photo control. It then added file APIs, folder maintenance, network/time settings and diagnostics. Asynchronous acceptance remained distinct from completion; boot-scoped tokens and control job IDs protect and identify operations.

Memory presentation evolved from an ambiguous small free-internal-memory number to separate chip SRAM, allocatable internal heap, minimum/largest blocks, PSRAM and RTC RAM. Power presentation distinguished USB host communication from AXP2101 external supply and charging stage. SD capacity gained GiB and used percentages.

Network cards gained ordered NTP servers, last attempt/success, RTC readback verification, manual synchronization and separately saved settings. Wi-Fi gained discovery, hidden SSID entry, password retention/open modes, reconnection rollback and AP provisioning. Success pages introduced actual remaining hotspot grace time, later reduced from 60 to 30 seconds. The AP address changed from the default 192.168.4.1 to 192.168.7.60 after a local Huawei management-address conflict.

Speech added independent configuration/test jobs; completed audio IDs replaced assumptions based on control acceptance. Recorded-pack endpoints were retired with ESP-TTS. Photo APIs gained create/rename/empty-delete, selected-photo display, import progress and format support. Versioned details and exact schemas are maintained in [file API](../http-files.md), not inferred from historical screenshots.

0.18.0 embeds the bilingual UI catalog and adds header locale preference without changing API payloads, enum values or SD schema. Stored user names/credentials are not translated.
