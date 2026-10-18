# Album and photo API

English | [简体中文](zh-CN/http-files.md) · [Documentation](README.md)

All file requests require the current `X-Album-Token` from `/api/status`. UTF-8 parameters are URL-encoded and decoded once. Access is limited to configured albums, including disabled albums. This is not an arbitrary SD browser: configuration, credentials and backups are not exposed. The token is not login authentication; use a trusted LAN.

## Files

| Method and path | Result |
|---|---|
| `GET /api/albums` | 200: `root`, configured `albums` IDs/directories/enabled flags/intervals |
| `GET /api/files?album=ID&offset=0&limit=50` | 200: naturally sorted `files`, `total/offset/limit/next_offset` |
| `GET /api/photo?album=ID&name=photo.bmp` | 200: unchanged `image/bmp`, chunked |
| `PUT /api/photo?album=ID&name=photo.bmp` | 201: `album/name/bytes/index_pending` |
| `DELETE /api/photo?album=ID&name=photo.bmp` | 200: `deleted/album/name/index_pending` |
| `POST /api/rescan` with `{}` | 202: control `job_id` |

Reject duplicate/unknown query parameters, NUL, path separators, `..`, hidden names, non-BMP names and FAT-reserved characters. Lists default to 50, allow 1–100 entries and offsets 0–2048; `next_offset:null` ends pagination. At most 2048 BMPs per album are listed. Invalid existing BMPs remain visible as `header_valid:false`; this flag checks header, dimensions and length, not every palette pixel. Directories/non-BMP/temporary files are excluded.

Unchanged `PUT /api/photo` uses raw BMP bytes, `Content-Length` and `image/bmp` or `application/octet-stream`, not multipart. Limit: **2 MiB**. Files must already meet [six-color playback format](image-format.md). Rotation 0/180 requires 800×480; 90/270 requires 480×800. No conversion occurs. Existing FAT-equivalent names return 409, leaving the original untouched.

An 8 KiB transfer buffer writes `.epaper-upload.tmp` in the target directory; flush/fsync, close and complete header/pixel validation precede rename. Bad/interrupted uploads discard the temporary file. Hidden leftovers after power loss do not play. Overall unchanged-upload budget is about 60 seconds; socket timeout 5 seconds. No range downloads or resumable chunks.

JSON errors expose `error/message`: 400 invalid parameter; 403 token; 404 missing album/photo; 409 busy/existing/limit; 413 too large; 415 media type; 422 invalid BMP; 507 space/write failure; other SD reads 500. Explicit `409 busy` can be retried after waiting. HTTP handles one transfer at a time; other requests can wait.

File success precedes application indexing. Check `control.files_revision/index_pending/index_error`; an increased revision with pending false and empty error confirms completion. Index errors do not undo file changes. Inspect and use rescan, rather than assuming the write failed. Current photo/deadline are retained if valid; deleting the current photo selects an enabled replacement. No remaining images keeps the screen and stops automatic updates. Uploading to a disabled album does not enable it.

SD transfer, configuration, decoding and display are mutually coordinated. Started transfers exit before shutdown/unmount. External USB power keeps services online; battery sleep makes APIs unavailable. Physical power loss, full-card FAT behavior and real hot removal during writes have not been exhaustively verified.

## Source conversion

`PUT /api/upload?album=ID&name=source.jpg` accepts raw standard JPEG, PNG or 24-bit BI_RGB BMP with token, length and matching image content type. No multipart. Input size **54–8,388,608 bytes inclusive**, maximum 12,000 pixels per side/40 MP. See [formats](image-format.md) for baseline JPEG, EXIF, PNG transparency/Adam7 and unsupported encodings.

Reception returns **202** with `job_id/name/state`. Background work converts, validates, publishes and indexes `<source-stem>.bmp` without replacing existing files. Different source extensions with the same stem conflict. Overall reception budget: 100 seconds; socket timeout: 20 seconds. Unknown network results must be checked before reupload.

`GET /api/upload` with token returns live status for the latest task this boot, independently of the cached status snapshot. Wait for `can_upload:true` before sending; it is a momentary precheck, so the actual request can still reject busy. Only one conversion runs. Folder maintenance and unchanged uploads are blocked while importing.

| Fields | Meaning |
|---|---|
| `job_id/album/source_name/name` | Job, album, received source name, output name |
| `active/published/state/error` | Running, published, phase, failure |
| `progress_percent` | Conversion phase progress; reception bytes are shown by the client |
| `elapsed_ms/conversion_ms/paused_ms` | Total including receive, post-receive duration, completed pauses |
| `decode_resize_ms/dither_ms/write_ms/validate_ms` | Phase times including waiting within each phase |
| `source_width/source_height/width/height` | Input/output dimensions |
| `workspace_bytes` | Upper bound of major conversion allocations, not whole-device heap |

States: `receiving/queued/decoding/resizing/dithering/writing/validating/publishing/indexing/waiting/succeeded/failed`. Waiting yields to announcement/display resources and resumes the prior phase. Succeeded means file and index complete. Failed with `published:true` means BMP already exists: inspect indexing/rescan.

`.epaper-import-source.tmp` and `.epaper-import-output.tmp` are hidden same-directory temporaries, cleaned after completion/failure and on boot. Publication follows full validation. Conversion keeps the device awake, runs at low priority and yields between short SD operations and processing rows; normal sleep planning resumes after cleanup.

### Browser composition and save-as

The browser reads `album.image_width/image_height/image_rotation_degrees`, reviews source orientation, physical frame placement, fit/crop and save name, then sends a target-size 24-bit BMP via `/api/upload`. The source 8 MiB limit is checked before browser decode; transmitted BMP is about 1.10 MiB. Task source metadata describes that BMP, not the computer original. Direct API sources still use automatic center crop.

The editor suggests `_2`, `_3`, etc. for existing/batch name conflicts and supports Unicode. Explicit `409 exists` refreshes names and returns unfinished composition drafts for a new confirmation; completed images are not resent. Explicit busy allows at most four sends after waiting. SD access errors and unknown transfer outcomes do not trigger automatic renamed retransmission. `500 sd_access_failed` is distinct from an existing destination. Original files are retained.

## Album folders

All JSON commands use the token and `Content-Type: application/json`; responses are asynchronous 202 control jobs.

```json
{"schema_version":1,"id":"world-asia","directory":"Asia","enabled":false,"interval_seconds":900}
```

`POST /api/album-create` creates a new directory and manifest entry. ID: 1–63 ASCII letters/digits/underscore/hyphen, excluding Windows reserved names. Directory defaults to ID; explicit Unicode directory: 1–120 UTF-8 bytes, no separators, controls/reserved characters, repeated dots, leading/trailing spaces, trailing dot or reserved names. Existing directories are not reused. Default disabled; interval 120–604800 seconds.

`POST /api/album-rename` uses `schema_version/id/directory`. It changes the physical folder and manifest directory, keeping stable ID, photo bytes, enabled flag, interval and current deadline. Current path updates without screen refresh. Existing targets, case-only renames, shared/nested configured directories are rejected. `<manifest>.rename-pending` records recovery intent. Boot restores interrupted renames according to the saved manifest; conflicts preserve evidence and stop startup for manual repair.

`POST /api/album-delete` uses `schema_version/id`. Only empty directories can be removed. Delete photos first. Hidden files, non-BMPs and subdirectories prevent deletion; no recursive delete. The web UI confirms permanent photo deletion, removes photos sequentially and finally deletes the empty album. It reports partial progress on interruption. Shared/nested albums are rejected. Deleting the final album leaves an empty manifest, retains screen pixels and stops automatic playback. Failed writes use rollback/backup recovery.

## Display a selected photo

`POST /api/show-photo`:

```json
{"schema_version":1,"album":"family","name":"001.bmp"}
```

Only these fields are accepted. Invalid/missing/corrupt photos fail without advancing the queue. Poll the matching control job until succeeded; NVS failure can occur after display and is reported. Busy upload/index/control work returns 409; announcement-time display can queue until resources are available.

Enabled-album selection continues from that photo's successor. Disabled-album selection changes only one display, retaining enabled selection; after that album's interval playback returns to the old queue. With no enabled images it stays. Repeating the same current content neither refreshes nor resets the deadline. Deep-sleep deadlines, restart recovery, scans and renames preserve valid manual positions; saving explicit playback selection ends a disabled-album manual override. Preview arrows never change device playback.

## Scan and setup

`POST /api/wifi-scan` starts a scan; `GET /api/wifi-scan` returns matching job/state/networks with SSID, RSSI, channel, authentication mode/open flag and truncation state. Use the token; do not confuse this job with control jobs. `POST /api/wifi-provision` with `{}` requests AP+STA setup. KEY can request it without a network. Setup uses `192.168.7.60`, blocks file operations, enables Wi-Fi/NTP on first successful save and keeps the AP for 30 seconds. See [setup](wifi-provisioning.md).

## Windows tools

```powershell
python tools/sd_photo_api.py --url http://192.168.1.100 albums
python tools/sd_photo_api.py --url http://192.168.1.100 create-album world-asia
python tools/generate_world_album.py --output ../world-landscapes-100
python tools/replace_album_library.py --url http://192.168.1.100 --backup ../old-library --backup-only
python tools/replace_album_library.py --url http://192.168.1.100 --source ../world-landscapes-100 --backup ../old-library
```

`sd_photo_api.py` uses the standard library, fetches tokens and waits for indexing. Downloads require a new local destination. `replace_album_library.py` verifies backups/new files, deletes old photos/empty albums, creates five disabled albums, uploads and downloads each for SHA-256 comparison, then enables all albums at 15 minutes. `installation.json` records verification. `--resume` rejects unknown albums/mappings/photos and validates existing files before skipping. Read-only downloads retry up to four times, discarding incomplete responses. Writes retry only explicitly rejected busy or confirmed-discarded incomplete receives; unknown timeouts are not blindly repeated.

Transfers use TCP_NODELAY, 32 KiB file buffers and 8 KiB network chunks; Wi-Fi stays in WIFI_PS_NONE while active. These do not prevent scheduled battery sleep after tasks finish.
