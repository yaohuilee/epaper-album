# Language support

English | [简体中文](zh-CN/language-support.md) · [Documentation](README.md)

## Web interface

Firmware 0.18.0 supports `en` and `zh-CN` in the status console, library, composition editor, preview, announcement controls and captive setup page. Use the header **Language / 语言** selector. The first visit follows the browser language (Chinese browsers select Simplified Chinese; other languages select English). A saved choice takes precedence; `?lang=en` or `?lang=zh-CN` explicitly selects a locale.

The choice is stored only in that origin's browser local storage as `epaper.language`; it is not written to SD or NVS. Setup and LAN addresses are different origins, so their preferences are independent. A storage-restricted captive browser still supports the URL parameter.

Switching reloads the current route and clears unsaved page drafts. The selector is disabled while a dialog is open or a page operation is active. Close a dialog or finish the operation before switching. The change does not submit configuration, change the current photo, restart the device or alter schedules.

Initial document labels and accessibility attributes are translated before application scripts execute. Dynamic messages use the same embedded catalog at their creation sites. The catalog is served locally at `/i18n.js`; no CDN, online translator or extra firmware library is needed. It changes display text only. API paths, schema keys, status values, SSIDs, passwords, album names and file names are not translated.

Dates remain `YYYY-MM-DD HH:mm:ss`, using device time at UTC+8. Compact durations omit zero units: `15 min`, `1 h 5 min`, `0 s`; Chinese uses `15分钟`, `1小时5分钟`, `0秒`. English UI does not imply an English time zone or English speech.

## Device screen and speech

The provisioning screen has bilingual fixed instructions with the same QR payloads and setup address. A browser-local UI choice cannot change a disconnected e-paper screen. Photos and on-screen album artwork are not translated.

ESP-TTS Xiaole is the current **Mandarin** voice. Time, temperature, humidity and fault announcements remain Mandarin in either UI language. English TTS is not implemented. Serial/diagnostic output and protocol error codes retain their existing technical form.

## Repository

`README.md`, the default `docs/` tree and root policy documents are English. Chinese guides are under `docs/zh-CN/`, with matching relative paths, and root policies use `.zh-CN.md`. Each guide links to the other language. Legal license texts are retained verbatim, not replaced by translated legal text. Historical English records are edited for reading while preserving version boundaries, decisions and verification limits; detailed original chronology is linked in Chinese.

## Maintenance and verification

Add UI messages to `components/services/i18n.js`, use `__t()` for dynamic text, and keep both locale paths covered in tests. Do not translate response enum values or user text. Preserve HTML markup in translated markup fragments, and avoid matching translations against arbitrary names.

`node tests/i18n_test.cjs` checks catalog coverage, locale selection/persistence behavior, static/accessibility translations, nontranslation of runtime user data, English UI paths and duration formatting. `python -X utf8 tools/check_docs.py` checks local links and English/Chinese counterpart coverage. Existing host UI and firmware tests remain required; English text length also needs desktop and narrow-screen browser checks.
