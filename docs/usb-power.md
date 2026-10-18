# USB, external power and sleep

English | [简体中文](zh-CN/usb-power.md) · [Documentation](README.md)

Since 0.14.34, computer USB, ordinary USB chargers and power banks all keep Wi-Fi and HTTP awake. Battery-only operation follows the original deep-sleep event schedule, normally after 10 idle seconds. An unknown power reading conservatively keeps the device awake. This policy is independent of language.

## Detection

AXP2101 supplies VBUS/power and battery/charging information. USB Serial/JTAG host communication detects the computer separately. A charger can supply VBUS without a USB host; USB host status alone cannot classify all external power. The web page displays both sources and charging phase.

An already sleeping ESP does not necessarily wake as USB is connected. Press KEY or wait for a scheduled timer. Keep-awake applies once running. Active transfers/synthesis finish cleanup before services and SD stop. Removing external power returns to battery scheduling.

## Limits

Charging phases are not measured current or a calibrated battery percentage. Battery capacity/runtime and deep-sleep current remain unmeasured. PWR long press is PMIC hardware shutdown; ESP timer wake cannot power it back on. E-paper retains pixels while power is absent. The RTC chip and RTC RAM are separate.

Offline announcements added scheduled wake before target time in 0.15.0. Preparation/playback prevents sleep; normal battery policy resumes afterward. Wake time comes from the external RTC, not a stale pre-sleep system clock. See [announcements](announcement.md) and [validation](validation.md).
