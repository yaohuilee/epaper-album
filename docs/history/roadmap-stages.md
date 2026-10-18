# Historical M0–M4 stages

English | [简体中文](../zh-CN/history/roadmap-stages.md) · [Current roadmap](../roadmap.md)

The early staged plan separated hardware/platform validation from photo playback and later services:

| Stage | Planned acceptance |
|---|---|
| M0 | Native Windows tools, chip/memory identification, pin/peripheral validation and recoverable backups |
| M1 | Validated SD configuration/BMP playback, ordering, restart/deep-sleep recovery and physical buttons |
| Later stages | RTC synchronization, environment/time announcements, power policies and optional activity detection |

Each stage required host tests and separate physical acceptance. No capacity, current, precision or driver success was to be inferred solely from a label or register setting. Configuration and state had separate persistence responsibilities; real destructive fault handling needed explicit validation.

The project subsequently added Web maintenance, provisioning, network library replacement, ESP-TTS, import conversion/composition and bilingual documentation. Activity detection and low-battery automation remain unimplemented. Historical checklists are context, not a promise that every proposed item shipped; consult current [validation](../validation.md) for completed measurements.
