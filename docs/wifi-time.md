# Wi-Fi and RTC synchronization

English | [简体中文](zh-CN/wifi-time.md) · [Documentation](README.md)

Wi-Fi serves HTTP and periodic NTP. Computer/external USB power keeps it online; battery wakes for due tasks and stops services before sleep. Saved-network failure does not enter provisioning or stop offline playback. External-power connection retries wait 60 seconds and defer around refresh/speech. NTP failures use the configured retry interval.

## Web controls

**Network & time** shows Wi-Fi, RSSI/channel, servers, last attempt/success, RTC and next sync. Manual synchronization, Wi-Fi settings and time settings belong to their corresponding cards. Dialog saves are independent and use SD atomic configuration updates. Saving time settings schedules the next cycle without immediately synchronizing. A Wi-Fi change may change IP and attempts rollback on failure. Passwords are never returned to the page.

Default NTP servers target mainland China. Server names are configurable, not bound to the interface language. See [control API](http-status.md).

## Optional build-time credential import

```powershell
python tools/import_wifi.py --help
```

Use the tool's documented input options for a local `WIFI.txt`. It generates ignored `.local/wifi_credentials.h` and `.local/wifi-provision.json`. Repository defaults do not embed private credentials. Enable `EPAPER_PROVISION_WIFI` explicitly in menuconfig only for a local build.

After validating SD settings/manifest, a local provision build merges network/time settings, writes and syncs a temporary file, renames the old root to `/config.json.pre-wifi` and installs the new configuration. Interrupted installation attempts backup restoration. An `album_state` marker imports identical built-in settings only once. It does not overwrite user edits at every boot. Existing conflicting backup/config combinations are rejected instead of overwritten. Invalid/unwritable SD leaves current valid playback or recovery mode intact.

## SD fields

Merge this fragment into a complete global file including `schema_version` and `album`:

```json
{
  "network": {"wifi_enabled": true, "ssid": "YOUR_SSID", "password": "YOUR_PASSWORD"},
  "time": {
    "rtc_storage": "utc",
    "sync": {
      "mode": "ntp",
      "servers": ["ntp.aliyun.com", "ntp1.aliyun.com", "ntp2.aliyun.com"],
      "interval_seconds": 86400,
      "retry_seconds": 900,
      "connect_timeout_seconds": 15,
      "sync_timeout_seconds": 20
    }
  }
}
```

Enable Wi-Fi together with `mode:"ntp"`; disable with false/`manual`. Interval: 3600–604800 s, retry: 60–86400 s, connect/sync timeouts: 5–60 s each. Use 1–3 servers, up to 127 bytes each.

## RTC and photo deadlines

NTP writes PCF85063 UTC, clears oscillator-stop and restarts it, then immediately verifies readback within 0–2 seconds. UTC migration is marked only after validation and persistence. Old device clocks previously written in local time are not silently interpreted as UTC before migration. Display/diagnostic device time is UTC+8, independent of UI language.

Clock jumps adjust photo and refresh-retry deadlines to retain remaining dwell time. Preserve `album_state` at `0x500000–0x51ffff` when upgrading; do not erase it. Wi-Fi uses RAM storage rather than relying on vendor default NVS credentials. Offline speech is implemented separately through Xiaole; it does not need NTP for every announcement.
