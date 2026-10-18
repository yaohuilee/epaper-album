# epaper-album

English | [简体中文](README.zh-CN.md)

An offline photo album for the **ESP32-S3 PhotoPainter six-color e-paper frame**. Photos live on a microSD card and can be managed using buttons or a local web interface. Playback and clock announcements use local files and the RTC; a previously configured device keeps playing when Wi-Fi is unavailable.

**Source version: 0.18.0.** ESP32-S3, 16 MB Flash, 8 MB PSRAM, 800×480 six-color display. Built with native Windows ESP-IDF v5.5.3. The web interface supports **English and Simplified Chinese**, with a language selector in the header.

## Start here

| Goal | Documentation |
|---|---|
| Set up Wi-Fi, upload photos and configure playback | [User guide](docs/user-guide.md) |
| Understand image sizes, orientation and conversion | [Photo format](docs/image-format.md) |
| Browse all documentation | [Documentation index](docs/README.md) |
| Build or update firmware on Windows | [Development](docs/development.md) |
| Review tested behavior and remaining work | [Validation](docs/validation.md) · [Roadmap](docs/roadmap.md) |

## Hardware and provenance

Developed and tested on the **Waveshare ESP32-S3-PhotoPainter**. The exact PCB revision of the test units has not been confirmed. See [hardware](docs/hardware.md) for measured results, pins and limits.

| Component | Target hardware |
|---|---|
| Processor and memory | ESP32-S3, 16 MB Flash, 8 MB PSRAM |
| Display | 7.3-inch E Ink Spectra 6, 800×480; black, white, red, yellow, blue and green |
| Storage and controls | microSD, USB Type-C, BOOT / KEY / PWR buttons |
| Clock and environment | PCF85063-series RTC, SHTC3 temperature/humidity sensor |
| Audio | Dual microphones, speaker connection, ES7210 / ES8311 |

Official hardware resources:

- [Product](https://www.waveshare.com/product/displays/e-paper/esp32-s3-photopainter.htm)
- [English wiki](https://www.waveshare.com/wiki/ESP32-S3-PhotoPainter) · [Chinese documentation](https://docs.waveshare.net/ESP32-S3-PhotoPainter/)
- [Schematic PDF](https://files.waveshare.com/wiki/ESP32-S3-PhotoPainter/ESP32-S3-PhotoPainter-Schematic.pdf)
- [Vendor examples and firmware](https://github.com/waveshareteam/ESP32-S3-PhotoPainter)

This is independently developed **third-party firmware**, unaffiliated with and not endorsed by Waveshare. Product names identify compatible hardware. Vendor firmware is not distributed here. Original code and documentation are MIT licensed; reused third-party components and resources are listed in [third-party notices](THIRD_PARTY_NOTICES.md).

*Hardware photo placeholder: an authorized photograph of the actual device will be added here.*

## Features

- **Offline playback:** sequential/shuffled photos, album-by-album or round-robin selection, restart recovery, night pause and manual photo selection.
- **Web photo library:** create, rename and delete albums; preview, download, delete and upload photos; select a photo to display on the frame.
- **Upload composition:** fit the whole photo or crop, rotate, zoom and drag; select landscape/portrait physical placement; edit the saved file name without renaming the source. Existing photos are never silently overwritten.
- **On-device conversion:** JPEG, PNG and 24-bit BMP imports become validated six-color BMPs, with progress shown in the page. Maximum source size: 8 MiB.
- **Wi-Fi and time:** open setup hotspot, captive setup page, scanned network list, periodic NTP synchronization, manual synchronization and external RTC verification.
- **Offline announcements:** hourly/half-hourly beeps and ESP-TTS Xiaole time, temperature and humidity speech, volume tests and independent quiet periods. **Speech remains Mandarin; selecting English changes the interface, not the voice.**
- **Power management:** remains online with computer USB or external USB power; scheduled deep sleep on battery.

Sound activity detection and automatic low-battery policies are not implemented. Device display time is fixed at UTC+8; language selection does not change the time zone.

## Everyday use

1. Install the firmware and insert an SD card with valid configuration and album manifest. See the [user guide](docs/user-guide.md).
2. On first setup, join the open hotspot shown on the e-paper display and open `http://192.168.7.60/setup`. After a successful connection, the hotspot remains available for 30 seconds.
3. Return to your home Wi-Fi and open the device address shown on the success page. The router assigns this address; it differs from the setup address.
4. Open **Photo library**, choose an album, and upload JPEG, PNG or uncompressed 24-bit BMP files. Review composition and save names before uploading. Enable albums and set intervals in **Playback settings**.
5. Use the header selector to switch between **English** and **简体中文**. The preference is stored in that browser. See [language support](docs/language-support.md).

The browser composes a target-size BMP; the device performs six-color dithering. Progressive/CMYK JPEG, WebP and HEIC need preprocessing with a [desktop tool](docs/image-format.md).

## Buttons and power

| Button | Short press | Long press |
|---|---|---|
| BOOT | Wake and show next photo | About 2 seconds requests deep sleep; ignored on external power |
| KEY | Wake / diagnostics; keep current photo | Hold about 2 seconds and release to enter Wi-Fi setup |
| PWR | Power on after hardware shutdown | About 6 seconds for hardware shutdown |

Plugging in USB does not immediately wake an already sleeping device. Press KEY or wait for its scheduled wake. The web page is unavailable during deep sleep. See [power](docs/usb-power.md).

## SD layout

```text
/
├── config.json          # Global device, Wi-Fi, time and display settings
└── albums/
    ├── albums.json      # Album selection, order and intervals
    ├── family/
    │   └── 001.bmp
    └── travel/
        └── 001.bmp
```

[sdcard/](sdcard/) contains configuration templates, without photos. The template enables only `family`, uses a 15-minute interval and starts with Wi-Fi disabled. See [configuration](docs/configuration.md). A completed sample workflow is documented in [100 world landscapes](docs/world-album-workflow.md).

## Source and licensing

`main/` handles boot and scheduling, `components/` implements hardware and services, `tools/` contains Windows build/test and image tools, and `tests/` contains host tests. See [architecture](docs/architecture.md).

This repository contains source, examples, tests, documentation and workflows. Credentials, personal media, original firmware dumps, generated packages and build output stay local. Before building, run `tools/Install-EspTts.ps1` to install the locally stored Xiaole dependency; see [ESP-TTS](docs/esp-tts.md).

Original code and documentation use the [MIT license](LICENSE), permitting use, modification, redistribution and commercial use with the required copyright and license notices. Third-party libraries, fonts and voice data retain their own terms. See [third-party notices](THIRD_PARTY_NOTICES.md), [contributing](CONTRIBUTING.md) and [security](SECURITY.md).
