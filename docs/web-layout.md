# Web console

English | [简体中文](zh-CN/web-layout.md) · [Documentation](README.md)

The console is an embedded header/sidebar/workspace layout, inspired by Ant Design grid conventions without downloading a UI framework. Navigation has at most two levels. Narrow screens stack cards and collapse the sidebar. The header includes the device clock and English/Chinese selector; see [language support](language-support.md).

## Pages and controls

- **Overview:** hardware, uptime, SD usage and internal/PSRAM/RTC memory details.
- **Network & time:** Wi-Fi and RTC/NTP cards with their own refresh/settings/sync buttons.
- **Environment & power:** SHTC3 and AXP2101 source/status facts.
- **Sound & announcements:** current phase, next target, voice settings/tests and separate quiet periods.
- **Playback settings:** saved album selection, dwell time and night pause; wide screens use adjacent cards.
- **Photo library:** Explorer-style album folders and photo area, with upload in the same workspace.

Refresh belongs at the top right of its workspace/card. Settings cards show saved summaries; upper-right Settings opens a modal. Dialogs have a single heading, unboxed X, explicit closure only, and Undo/Save at bottom right. Escape and backdrop do not close them. Results use floating notifications rather than inserting vertical space.

## Library and preview

Album create/rename/delete and file upload use the toolbar. Opening folders changes management scope, not playback selection. Photo cards offer actual device display, preview, download and confirmed permanent delete. Failed/partial operations preserve counts and report their state. Status notifications do not shift the grid.

Sidebar controls share aligned labels for current album, current photo and next-photo countdown. Long filenames ellipsize with hover title. Click the filename for the same preview used by the library. Next-photo progress appears inside the existing button without layout movement.

Preview reserves known aspect/size space, fits the full image within the viewport and needs no redundant bottom Close button. X closes. Arrows browse the current album without changing device playback. It displays raw stored panel BMP orientation, not the original composition orientation.

## Wi-Fi dialogs and time

Networks scroll inside a bounded area while the footer stays visible. SSID supports discovery and hidden manual entry. One stable password field handles a new network, retaining a password or open mode; it does not unexpectedly insert a second input. Settings summaries and saves remain separate.

All timestamps use device RTC and `YYYY-MM-DD HH:mm:ss`, fixed UTC+8. Header/time card tick each second. Photo/time-sync/announcement countdowns and uptime also tick each second, omitting zero duration units. Missing RTC does not fall back to computer time. Capacity uses GiB above 1 GiB, otherwise MiB/KiB, with SD percent used. Internal RAM and PSRAM are distinct statistics. Notes use short factual sentences.

## Import review

Sources are selected together, then individually reviewed locally before transmission. Fit/white margins is default. Physical frame rotation is independent from image rotation. Fill supports drag and zoom. Each draft retains its composition and final name. An existing name receives a unique suggestion; live validation disables invalid confirmation. Saved sources remain untouched.

The browser encodes final-size BMP; device converts and reports receive/decode/resize/dither/write/validate/index phases. Busy periods pause processing. Unknown transfer results stop rather than blind retry; explicit name conflicts restore unfinished recipes for confirmation. English and Chinese use the same controls, payloads and functional logic.
