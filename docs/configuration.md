# Configuration (schema v1)

English | [简体中文](zh-CN/configuration.md) · [Documentation](README.md)

`/config.json` at the SD root holds global device settings. `/albums/albums.json` holds album selection and playback settings. The global configuration may select a different album root/manifest. Runtime state is stored in NVS, not written into user configuration. Web settings update their corresponding SD file independently.

Paths are relative to the SD filesystem; do not include the firmware mount point `/sdcard`. JSON is UTF-8 without comments or trailing commas. Start from [templates](../sdcard/).

## Album rules

- Only explicitly listed `enabled: true` albums play. Creating a directory does not enable it.
- `directory` is relative to the album root; absolute paths, `..` and duplicate IDs are rejected.
- `recursive` is currently false. Direct BMP children are scanned; a nested grouping needs another explicit album entry.
- `.bmp` matching is case-insensitive. Natural filename sorting puts 2 before 10.
- `order` is `sequential` or `shuffle`. A shuffled round has no repeats; the first image of a new round differs from the previous last image when more than one exists.
- `album_strategy` defaults to `album_by_album`: finish an enabled album, then advance in manifest order. `round_robin` takes one photo from each enabled album, maintaining each cursor independently.
- Each photo uses its album's `interval_seconds`, falling back to `playback.default_interval_seconds`. Timing starts after refresh completion, excluding the roughly 30-second refresh.
- Recommended interval: 900 seconds. Accepted range: 120–604800 seconds.
- Empty albums and bad images are skipped. If no valid images remain, keep the current screen and stop automatic refresh.
- Recovery uses stable album ID and relative photo path, not an obsolete array index.

### Night pause

Stored as `playback.night_pause` in the manifest; saved separately in the web page:

```json
"night_pause": {"enabled": false, "start": "22:00", "end": "06:00"}
```

Older configurations default off. Times are strict device-time `HH:mm`; start and end must differ. The start is inclusive and the end exclusive. Start before end is same-day; start after end crosses midnight. The UI offers 15-minute steps but retains existing off-grid values. The API/file accepts valid individual minutes.

The pause prevents new automatic changes. Manual, initial and recovery displays still run; an existing refresh is not interrupted. At the end, change once if overdue, otherwise retain the original deadline. Saving pause settings alone does not change the photo, selection or deadline. Battery wake scheduling skips paused photo deadlines but can still wake for time sync or buttons. Invalid system time disables night pause.

### Ordering and manual changes

For album-by-album, two family photos and three travel photos play `family1 → family2 → travel1 → travel2 → travel3 → family1`. Disabled/empty albums are skipped. Shuffle changes within-album order, not album order. Round-robin repeats smaller albums more often.

BOOT advances by the active strategy. A successful manual change starts a full interval at refresh completion, replacing the old deadline. Failed validation or refresh does not commit the new photo or reset timing. A sole valid photo is not repeatedly refreshed; a manual operation may restart its interval.

### Restart and deep-sleep recovery

With `resume_position: true`, recover the last successfully shown ID/path. If the retained screen matches saved state, do not refresh it again. An interrupted/uncertain display is refreshed before timing resumes. A full restart starts a new full interval; it does not catch up for time powered off. Deleted, disabled or invalid saved positions fall back to the first valid selection. With no valid selection, retain screen pixels and stop automatic changes. `resume_position: false` starts from the first valid photo.

Normal timer/button deep-sleep wake retains the original deadline. Otherwise periodic wakeups would continually postpone playback. Cross-restart shuffle recovery also stores the seed/order cursor; a photo path alone cannot restore a shuffled round.

## Global settings

| Field | Behavior |
|---|---|
| `schema_version` | 1 in both files; unknown versions rejected |
| `album.root/manifest` | Album root and manifest, without `/sdcard` |
| `display.rotation_degrees` | 0/180 landscape, 90/270 portrait input |
| `display.on_invalid_image` | Only `skip` is supported |
| `network.wifi_enabled/ssid/password` | Wi-Fi activation and credentials, coordinated with NTP |
| `time.rtc_storage` | UTC; initial migration confirmed by successful synchronization |
| `time.sync` | Ordered NTP servers, interval, retry and timeout; see [time](wifi-time.md) |
| `power.sleep_between_events` | Defaults true; external-power keep-awake takes precedence |
| `power.low_battery_policy` | Only `none` is supported |

`activity.enabled` must stay false. Device metadata, arbitrary `device.timezone` and sensor calibration offsets are reserved; display time remains UTC+8 and calibration offsets are not applied.

Announcements use fixed `engine=esp-tts`, `voice=xiaole`, `speech_speed=2`. Supported fields include `minutes` 0/30, `volume_percent`, temperature/humidity inclusion, temperature decimals 0/1, and up to eight pause periods. Defaults: off, volume 40%, pause 22:00–06:00. Legacy recorded-pack fields migrate atomically on boot. See [announcements](announcement.md).

Language preference is browser-local. There is no language field in the SD schema.

## Safety and fallback

Each JSON is limited to 16 KiB, nesting to 32 levels, albums to 64 and header-valid photos to 2048 total. File types, ranges and paths are checked. Unknown fields produce warnings; invalid core fields reject the file. Apply only completely validated configurations, retaining a last-valid cache. With no valid configuration, use conservative recovery with automatic announcements off.

Updates use temporary files and rename/backup recovery. SD removal during operation is discouraged; errors stop related tasks. Credentials reside on SD and in local NVS cache, or ignored local provisioning files; never publish them. Examples use placeholders only.
