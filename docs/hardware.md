# 目标硬件

[文档首页](README.md) · [项目首页](../README.md)

2026-10-09 对目标设备独立探测，硬件与 Waveshare ESP32-S3-PhotoPainter 一致；未拆机确认所有丝印。以下区分实测与规格，测试结果不代替驱动验收。

| 外设 | 识别与验证 |
|---|---|
| MCU | ESP32-S3 rev 0.2，240 MHz，40 MHz 晶振 |
| Flash / PSRAM | 16 MB / 8 MB；7 MiB PSRAM 三轮模式测试零错误 |
| 电子纸 | 实测 800×480、黑白红黄蓝绿；全刷约 30.3 秒；7.3 英寸来自资料 |
| 电源 | AXP2101，I2C 0x34；长按 PWR 关机和短按开机通过 |
| RTC | PCF85063 系列，0x51；走时、STOP、报警/计时标志、主电源循环保持通过 |
| 温湿度 | SHTC3，0x70；ID、温湿度读取及 CRC 通过 |
| 播放音频 | ES8311，0x18；一个扬声器三音测试现场确认 |
| 录音音频 | ES7210，0x40；两路麦克风采到有效信号 |
| SD | 4 线 SDMMC；现有约 15.63 GB 卡只读挂载和重复读取通过 |

## 引脚

| 接口 | GPIO |
|---|---|
| I2C SDA/SCL | 47 / 48 |
| EPD MOSI/CLK/DC/CS/RST/BUSY | 11 / 10 / 8 / 9 / 12 / 13；BUSY 低有效 |
| I2S MCLK/BCLK/WS/TX/RX | 14 / 15 / 16 / 17 / 18 |
| 功放使能 | 7 |
| SD CLK/CMD/D0/D1/D2/D3 | 39 / 41 / 40 / 1 / 2 / 38 |
| BOOT/KEY | 0 / 4，低有效 |
| PWR SYS_OUT / PMIC IRQ | 5 高有效 / 21 |
| LED | 42 / 45 |

## 已发现的驱动注意事项

电源循环后 AXP2101 屏幕电源 ALDO4 会恢复默认 1.8 V；必须在屏幕初始化前按板级要求配置为 3.3 V。使用正式驱动 API 与寄存器核对，不沿用探测脚本的 GPIO matrix 直接寄存器写法。

音频链路必须配置正确 MCLK 和 DAC 增益。首次低音量测试听不清，提高音量并配置 6.144 MHz MCLK 后三音正常；不能只初始化 BCLK/WS 就认定 DAC 可用。

AXP2101 ADC 需开启相应通道并等待采样稳定。扩展测试电池 3.68 V、VBUS 4.881 V；配置充电恒流 500 mA、目标 4.2 V，设置值不等于实际电流。用户报告电池标签 3.7 V / 1500mA，实际 mAh 容量和续航未验证。充电参数不因新固件随意增大。

RTC 主电源循环保持时 USB 与锂电池仍连接；移除所有供电、纽扣电池容量、报警 IRQ 接线和长期 ppm 精度尚未验证。原机日志中的内部 RTC 定时唤醒不等同于外部 RTC 报警唤醒。

## 参考资料

- [板级原理图](https://files.waveshare.com/wiki/ESP32-S3-PhotoPainter/ESP32-S3-PhotoPainter-Schematic.pdf)
- [厂商源码](https://github.com/waveshareteam/ESP32-S3-PhotoPainter)
- [产品教程](https://spotpear.cn/wiki/ESP32-S3-7.3-inch-E-Paper-Ink-Display-E6-Full-Color-PhotoPainter-xiaozhi-AI.html)
- [AXP2101 数据手册](https://files.waveshare.com/wiki/common/X-power-AXP2101_SWcharge_V1.0.pdf)

完整 Flash 备份和原始探测日志保留在用户本机，本仓库仅保存后续开发需要的硬件摘要。
