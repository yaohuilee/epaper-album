# Web layout evolution

English | [简体中文](../zh-CN/history/web-layout-changes.md) · [Current console](../web-layout.md)

The initial single flat status page became a header, two-level sidebar and grid-based workspace. Refresh moved to top-right workspace/card actions. Playback operations moved to sidebar bottom; wide-screen setting cards became adjacent, stacking on narrow screens.

Saved settings became compact cards with upper-right Settings buttons. Dialogs use a single title, unboxed X, explicit dismissal and lower-right Undo/Save. Escape/backdrop dismissal was disabled. Floating result messages removed content jumps and excessive bottom whitespace. Wi-Fi lists use bounded internal scrolling and one stable password input.

Library browsing/upload merged into an Explorer-style folder workspace. Sidebar current album/photo/countdown share aligned labels; long names ellipsize. Next-photo progress stays inside its button. Clicking the current filename opens the same responsive preview as library cards. Preview preallocates aspect space, removes the redundant bottom Close button and adds previous/next that never affect playback.

All dates became `YYYY-MM-DD HH:mm:ss` from device time. Header, time card, uptime and countdowns tick each second; zero duration units disappear. Status-update timestamps stopped using computer time. GiB and SD percentages improve capacity presentation. Notes became concise factual sentences.

Upload review later added orientation, fit/crop, physical frame placement and save-as names. 0.18.0 added English/Chinese headers, locale-aware dynamic messages and compact duration units, keeping APIs and user data unchanged. See [composition history](photo-import-composition.md) for version boundaries and [validation](../validation.md) for actual testing rather than historical design intent.
