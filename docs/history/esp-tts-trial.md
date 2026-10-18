# ESP-TTS voice trials

English | [简体中文](../zh-CN/history/esp-tts-trial.md) · [Current build](../esp-tts.md)

0.15.3 tested small official on-device Chinese speech on the ESP32-S3. The user confirmed complete, intelligible time/temperature/humidity output, then requested comparison of official voices with a bright, full female sound.

At 40% volume and speed 2, A was Xiaoxin Lite, B Xiaole, C full Xiaoxin. All three played; the user selected **B: Xiaole**. Libraries and voice resources remained local official dependencies, not repository-distributed binaries.

0.16.0 removed the recorded-pack path and unified automatic/test speech with Xiaole. Complete phrases replaced separately assembled recordings. Hourly/half-hourly preparation measured 4213/4342 ms; user confirmation covered complete phrases and natural pauses. The fixed data is 2,938,039 bytes in a 4 MiB Flash partition, synthesized as 16 kHz mono PCM then resampled to 24 kHz.

Those immediate tests do not establish absolute timed onset, long-term battery power or real quiet-period boundary triggering. CPU/heap/concurrency measurements followed in [0.16.1 stress tests](../performance-stress.md). Official license scope and installation are documented in [ESP-TTS](../esp-tts.md); UI English support does not add an English voice.
