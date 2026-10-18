# Target hardware

English | [简体中文](zh-CN/hardware.md) · [Documentation](README.md)

Independent probing on 2026-10-09 matched Waveshare ESP32-S3-PhotoPainter hardware. Not every silkscreen/PCB revision was checked. Measurements below are not substitutes for full driver acceptance.

| Peripheral | Identification and verification |
|---|---|
| MCU | ESP32-S3 rev 0.2, 240 MHz, 40 MHz crystal |
| Flash / PSRAM | 16 MB / 8 MB; 7 MiB PSRAM passed three pattern-test rounds |
| E-paper | Measured 800×480, six colors; full refresh about 30.3 s; 7.3-inch size from documentation |
| Power | AXP2101, I2C 0x34; long PWR shutdown / short power-on verified |
| RTC | PCF85063 family, 0x51; ticking, STOP, alarm/timer flags and main-power cycling verified |
| Environment | SHTC3, 0x70; ID, readings and CRC verified |
| Speaker | ES8311, 0x18; three-tone playback heard by user |
| Microphones | ES7210, 0x40; valid signals from two channels |
| SD | Four-bit SDMMC; approximately 15.63 GB test card mounted and repeatedly read |

## Pins

| Interface | GPIO |
|---|---|
| I2C SDA/SCL | 47 / 48 |
| EPD MOSI/CLK/DC/CS/RST/BUSY | 11 / 10 / 8 / 9 / 12 / 13; BUSY active low |
| I2S MCLK/BCLK/WS/TX/RX | 14 / 15 / 16 / 17 / 18 |
| Amplifier enable | 7 |
| SD CLK/CMD/D0/D1/D2/D3 | 39 / 41 / 40 / 1 / 2 / 38 |
| BOOT/KEY | 0 / 4, active low |
| PWR SYS_OUT / PMIC IRQ | 5 active high / 21 |
| LED | 42 / 45 |

## Driver findings and limits

AXP2101 ALDO4 resets to 1.8 V after a power cycle and must be configured to the board-required 3.3 V before display initialization. Use the production driver/API and register checks, not exploratory GPIO matrix writes.

Audio requires correct MCLK and DAC gain. The initially quiet tone test became audible with gain configuration and 6.144 MHz MCLK. BCLK/WS initialization alone does not prove a working DAC.

AXP2101 ADC channels need enabling and settling time. Extended probing measured battery 3.68 V and VBUS 4.881 V. Configured 500 mA current and 4.2 V target are not measured charging current. The user reported a battery label of 3.7 V / 1500mA; mAh capacity and runtime remain unverified. Production status does not provide a calibrated remaining-charge percentage.

RTC main-power-cycle tests retained USB/lithium supply. Full removal of all power, backup-cell capacity, alarm IRQ wiring and long-term ppm accuracy remain untested. ESP internal sleep timer wake is not evidence of external RTC alarm wake.

## References

- [Official schematic](https://files.waveshare.com/wiki/ESP32-S3-PhotoPainter/ESP32-S3-PhotoPainter-Schematic.pdf)
- [Vendor source](https://github.com/waveshareteam/ESP32-S3-PhotoPainter)
- [Product tutorial](https://spotpear.cn/wiki/ESP32-S3-7.3-inch-E-Paper-Ink-Display-E6-Full-Color-PhotoPainter-xiaozhi-AI.html)
- [AXP2101 datasheet](https://files.waveshare.com/wiki/common/X-power-AXP2101_SWcharge_V1.0.pdf)

Flash backups and raw probe recordings remain local. This repository includes only the hardware summary needed for development.
