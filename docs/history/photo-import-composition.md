# Photo composition and upload history

English | [简体中文](../zh-CN/history/photo-import-composition.md) · [Current format](../image-format.md)

## 0.17.4 — Review before upload (2026-10-18)

A 1279×1706 portrait without EXIF was automatically center-cropped into a landscape panel, losing upper/lower subject content. Browser review changed the default to full-photo white margins, added 90° rotation, fill/crop, zoom and dragging, and deferred all transfer until batch confirmation. The browser sends target-size 24-bit BMP; the device still performs six-color conversion. Direct source API behavior remains center crop.

The real source converted in 11.106 seconds. Downloaded output was 800×480, six palette colors with white side margins. User-selected full composition and previous cropped version coexisted. Host geometry/BMP/navigation/cancel tests and desktop/narrow layouts passed. Screen display completed; no new physical-screen visual acceptance was claimed at that point.

## 0.17.5 — Physical frame placement

Added physical F separately from source rotation and firmware R. Portrait review uses 480×800; uploaded data rotates by `−(F+R)` to reconstruct upright composition after driver and physical rotation. Integer copies avoid interpolation/mirroring. No per-photo metadata or global orientation rewrite.

591 checks covered all sixteen F/R combinations and asymmetric pixel reconstruction. A real clockwise portrait converted in 11.136 seconds; BMP readback, pixel restoration, margins and layouts passed. Other combinations remained host-tested rather than individually flashed/visually confirmed.

## 0.17.6 — Friendly duplicate errors

Paged preflight compared output names case-insensitively, replacing the ambiguous English “exists or inaccessible” with distinct name and SD access failures. Zero successes no longer displayed a 0/1 summary. Fourteen import scenarios passed. A real duplicate returned 409 without starting a job, and original bytes were unchanged. This preflight blocking behavior was superseded by editable names in 0.17.7.

## 0.17.7 — Save-as inside the page

Recipes retain final BMP names, suggest unused `_2/_3` suffixes and validate Unicode byte limits, reserved names and same-batch/existing conflicts. Renaming does not modify source Files. Late explicit exists errors return unfinished recipes to review; completed photos are not resent. Explicit busy retries are bounded to four sends; unknown network/SD failures stop.

621 editor checks, twenty import scenarios and folder/display regression passed. COM7 saved a Chinese-named second photo in 11.121 seconds. Complete 800×480 six-color output readback succeeded; original before/after SHA-256 matched. Photo count changed from one to two; current photo, album flags and 86400-second interval stayed unchanged. A weak-link readback initially interrupted; existing read-only retries completed after stopping browser polling. No repeated upload occurred.
