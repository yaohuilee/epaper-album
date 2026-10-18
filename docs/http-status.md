# HTTP status and control

English | [简体中文](zh-CN/http-status.md) · [Documentation](README.md)

The LAN server uses port 80 at a router-assigned IP, printed on serial and the setup success page. Computer/charger/power-bank operation stays online; battery deep sleep makes HTTP unavailable. A configured device can operate offline without entering setup. Unconfigured or manually requested setup uses `http://192.168.7.60/setup`.

| Route | Purpose |
|---|---|
| `GET /` | Device console and settings |
| `GET /albums` | Photo library |
| `GET /setup` | Captive Wi-Fi setup |
| `GET /api/status` | Read-only device snapshot |
| `GET /i18n.js` | Embedded English/Chinese UI catalog |

The interface language does not change API fields, enums or payloads.

## Command contract

Read `control.token` from `/api/status`. Control POSTs require `X-Album-Token` and `Content-Type: application/json`; body limit 4096 bytes. The token changes on boot. It protects requests, not user login. The server does not allow cross-origin reads and belongs on a trusted LAN.

Asynchronous commands return **202** and `job_id`, meaning accepted, not completed. Poll `control.job_id/state/error/busy`, match the request ID and wait for `succeeded` or `failed`. Only the latest command result is retained; restart clears it. Malformed requests generally return 400/415, token failures 403 and busy/unavailable commands 409. Semantic/storage failures can occur after acceptance. An uncertain response requires checking status before repeating writes.

## Playback

`POST /api/next` takes `{}`. `POST /api/settings` accepts one or more of `album_ids`, `interval_seconds`, `night_pause` with `schema_version:1`:

```json
{"schema_version":1,"album_ids":["family","travel"],"interval_seconds":900}
```

Unknown/duplicate IDs, empty album selections and invalid intervals are rejected. Selecting albums alone preserves all intervals. Saving interval alone changes the default and enabled albums, retaining disabled-album intervals. Range: 120–604800 whole seconds, counted after refresh completion.

```json
{"schema_version":1,"night_pause":{"enabled":true,"start":"22:00","end":"06:00"}}
```

Night pause saves independently without changing selection, interval or current deadline. All three fields are required. Device-time `HH:mm` uses inclusive start/exclusive end. Start before end is same-day, start after end overnight, equal rejected. API accepts individual minutes even though the UI uses 15-minute steps. Defaults off, preset 22:00–06:00. State appears as `album.night_pause/night_paused`; `next_seconds` includes pause delay. Manual next remains available.

Validated temporary-file/backup replacement and index rebuilding perform saves. A later display/NVS failure may occur after configuration was saved; errors identify this partial result.

## Time

`POST /api/time-sync` takes `{}`. `POST /api/time-settings` saves all three fields together:

```json
{"schema_version":1,"servers":["ntp.aliyun.com","ntp1.aliyun.com"],"interval_seconds":86400,"retry_seconds":900}
```

Use 1–3 distinct nonempty hostnames/IPv4 addresses, 1–127 characters each, no URL scheme, port or path. Interval: 3600–604800 seconds. Retry: 60–86400 seconds. Unknown fields, duplicate keys and noninteger values are rejected. Disabled synchronization rejects these requests.

Success requires NTP response, RTC write/readback and persistence. Success uses the normal interval; failure uses retry. Saving settings starts a new interval now, without immediate synchronization. Only three `time.sync` fields change; credentials, timeouts and unrelated settings remain. Photo remaining time survives clock jumps.

## Wi-Fi

`POST /api/wifi-settings`:

```json
{"schema_version":1,"ssid":"YOUR_SSID","password":"YOUR_PASSWORD"}
```

SSID is case-sensitive, 1–32 UTF-8 bytes, on 2.4 GHz. Omit password to retain it; an empty string selects an open network. New passwords are 8–63 bytes or a 64-digit hexadecimal PSK. Only these fields are accepted, without duplicate keys.

The SD root configuration is saved before reconnecting. The IP may change. A failed connection attempts to restore previous settings/network; manual setup or SD editing can recover if restoration fails. The password is not returned in status. See [scan/setup API](http-files.md).

## Snapshot data

The application samples a locked snapshot. During refresh/synchronization some fields may retain the previous sample; read failures appear unavailable. `X-Status-Age-Ms` indicates age. The UI advances RTC-based time and countdowns using snapshot age and monotonic elapsed time, not computer wall-clock time.

| Object | Main fields |
|---|---|
| `device` | Version, MAC, Flash, internal/PSRAM heap, uptime |
| `network` | IP, Wi-Fi, USB host, setup, NTP settings/results |
| `rtc` | Chip, validity, UTC basis, calendar |
| `environment` | SHTC3 readings and validity |
| `power` | AXP2101, VBUS, battery presence and charging stage |
| `album` | Counts, current image, interval/deadline, import dimensions and decoding rotation |
| `control` | Token, job, busy flag, file revision and indexing |
| `image_import` | Cached latest conversion status |
| `announcement` | Engine/voice, phase, next target and test completion |

Epoch fields are UTC seconds; zero means no trusted record. Last successful synchronization persists; attempts/errors cover this boot. Hardware SRAM is 512 KiB, RTC SRAM 16 KiB; neither equals free heap. Internal and PSRAM heaps expose total, free, minimum and largest block separately. Charging stage does not measure current or charge percentage.

Announcements use `/api/announcement-settings`, `/api/announcement-pause-settings` and `/api/announcement-test`. Actual test completion uses `completed_test_id/last_test_result`, not command acceptance. Recorded voice-pack endpoints were removed. Optional diagnostics are off by default; see [performance](performance-stress.md). Historical protocol notes are in [history](history/http-status-changes.md).
