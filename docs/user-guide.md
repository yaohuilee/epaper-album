# User guide

English | [简体中文](zh-CN/user-guide.md) · [Documentation](README.md)

## 1. Prepare the SD card

Copy the configuration templates from [sdcard/](../sdcard/) to the SD root. Keep `/config.json` and `/albums/albums.json` in their respective locations. Add prepared BMP photos to album folders, or upload photos after configuring Wi-Fi. The firmware does not format the card. Configuration uses UTF-8 JSON without comments or trailing commas.

The default manifest enables `family` with a 900-second interval. It contains no photos. Refer to [configuration](configuration.md) for explicit album inclusion, paths and recovery behavior.

## 2. Connect Wi-Fi

On first setup, or after holding KEY about 2 seconds and releasing it, the display shows an open device hotspot. Use the QR scanner in your phone's **WLAN settings** to join it. The second QR code opens the web page and can be scanned with a camera or WeChat. A captive setup page may open automatically; otherwise open `http://192.168.7.60/setup`.

Select a visible 2.4 GHz network or enter a hidden SSID. Enter its password and choose **Save & connect**. A successful page shows the new LAN address and a 30-second hotspot countdown. No repeated submission is needed. Return the phone to the same home Wi-Fi to open the device address.

A device with saved credentials keeps playing if its network becomes unavailable; it does not automatically replace photos with the setup screen. See [setup](wifi-provisioning.md).

## 3. Maintain photos

Open **Photo library**. Albums appear as folders. Create, rename or delete albums using the toolbar. New albums are not enabled for playback automatically. To delete a nonempty album, confirm deletion of its photos; unlisted files and nested folders prevent final folder deletion.

Open an album to browse up to 50 photos per page. Filtering applies only to the current page. Preview arrows browse the album in the browser without changing the device display. **Display this photo** requests an actual screen update. Delete permanently removes the selected BMP; download retrieves its stored bytes.

For uploads:

1. Choose or drop standard JPEG, PNG or uncompressed 24-bit BMP, up to 8 MiB per source.
2. Review each photo. The default fits the complete photo with white margins. Choose the physical frame orientation separately from photo rotation. Fill mode supports 100–400% zoom and dragging the crop.
3. Edit **Save file name** when needed; `.bmp` is fixed. Existing/batch duplicates get an unused suggestion such as `photo_2.bmp`. Unicode names are supported. Invalid or duplicate names disable confirmation.
4. Confirm the batch. The browser generates each composed 24-bit BMP; the device dithers to six colors, validates and indexes it. The page shows transfer and conversion progress.

No image is sent before batch confirmation. Original files on the computer/phone are unchanged. Existing device photos are not overwritten. Resetting composition keeps the save name. Previous/next retain per-photo drafts. A conflict discovered during saving returns unfinished photos to review with their composition retained. Completed photos are not sent again; cancel keeps unfinished selection.

Explicit busy rejection has a bounded retry. An unconfirmed network response does not cause automatic reupload: restore the connection and check the task and file list first. See [API](http-files.md).

## 4. Configure playback

In **Playback settings**, each card shows saved values. Its upper-right **Settings** opens a dialog. Undo and Save are at the lower right; X closes without saving. Escape and the backdrop do not close dialogs.

Save album selection, photo interval and night pause independently. Intervals range from 2 minutes to 7 days and start when a screen refresh finishes. Album selection does not change simply because you opened a folder in the library.

Night pause defaults off with 22:00–06:00 preset. Start is inclusive, end exclusive. Start before end means the same day; start after end means overnight; equal times are rejected. The UI uses 15-minute steps and preserves preexisting off-grid values. Automatic changes pause, but manual changes and initial/recovery display remain possible.

## 5. Time, status and language

**Network & time** displays Wi-Fi details, ordered NTP servers, attempts and successes, RTC state and a manual sync button. Wi-Fi settings and sync settings save separately to SD. Time sync settings do not initiate an immediate sync. A failed Wi-Fi change attempts to restore the previous network.

Header time, time card, update timestamps, uptime and countdowns share the device RTC time source. Dates use `YYYY-MM-DD HH:mm:ss`. Durations omit zero units. Device time is UTC+8; an English interface does not change the time zone. Internal heap and PSRAM statistics are separate; hardware capacity is not free heap.

Select **English** or **简体中文** in the header. The browser stores the choice and reloads the current workspace. Close dialogs first; active operations prevent switching. Unsaved page drafts are cleared on reload. Names, passwords, files, settings, playback and time remain unchanged. See [language support](language-support.md).

## 6. Power and buttons

| Action | Behavior |
|---|---|
| BOOT short | Wake and next photo |
| BOOT held about 2 s | Request deep sleep; ignored on external power |
| KEY short | Wake / diagnostics; retain photo |
| KEY held about 2 s, then released | Wi-Fi setup |
| PWR short | Power on after hardware shutdown |
| PWR held about 6 s | Hardware shutdown |

Computer USB, charger and power bank keep Wi-Fi and HTTP online. Battery operation sleeps between scheduled events. Connecting USB to a sleeping device does not necessarily wake it; press KEY. E-paper keeps its image without power. Hardware shutdown cannot be undone by the ESP deep-sleep timer. See [power](usb-power.md).

## Sound and announcements

Enable hourly/half-hourly announcements and volume under **Sound & announcements**. Tests use the draft volume without saving. Set up to eight independent daily quiet periods; these are separate from album night pause. The target time decides whether a period blocks speech, so a 06:00 resumption allows 05:59:56 countdown beeps.

Xiaole synthesizes Mandarin offline, even in the English interface. No prerecorded SD pack is required. See [announcements](announcement.md).

## Troubleshooting

- Offline page: check Wi-Fi, external power and sleep state. Press KEY if needed.
- Upload uncertain: check the conversion task and library before retrying.
- Missing photo: verify enabled albums and [BMP format](image-format.md).
- Preview appears sideways: it shows raw panel BMP coordinates. Physical frame orientation is applied when composing new files.
- File changes succeeded but indexing failed: inspect the error and rescan; do not assume the write was undone.
