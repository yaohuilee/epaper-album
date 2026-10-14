# USB、外部供电与休眠（0.14.34）

[文档首页](README.md) · [项目首页](../README.md)

以下为默认 `power.sleep_between_events: true` 的行为。连接电脑、普通 USB 充电器或充电宝时，设备保持唤醒，Wi-Fi 和 HTTP 服务保持运行。相册自动换图和周期 NTP 按原计划执行。外部供电期间忽略 BOOT 的软件睡眠请求，PWR 硬件长按关机仍有效。

仅使用电池且无 USB 主机时，按原有空闲窗口、换图和校时计划深睡。断开电脑后保留3秒 USB 数据帧消失确认期；外部供电状态最多每2秒更新一次，真正进入深睡前立即重读电源状态。断电转换不重置照片或校时截止时间。

## 状态来源

- `usb_serial_jtag_is_connected()` 根据主机 SOF 数据帧检测数据连接，不要求电脑打开串口。接口来自 [ESP-IDF v5.5.3](https://github.com/espressif/esp-idf/blob/v5.5.3/components/esp_driver_usb_serial_jtag/include/driver/usb_serial_jtag.h)。USB 主机字段仍独立表示数据连接，充电器不被标记为电脑。
- AXP2101 `0x00` 的 bit 5 表示 VBUS 有效，bit 3 表示电池存在；`0x01` 的 bits 6:5 表示电池电流方向，bits 2:0 表示充电阶段。只读状态，不修改充电参数。[X-Powers 数据手册](https://files.waveshare.com/wiki/common/X-power-AXP2101_SWcharge_V1.0.pdf)。
- VBUS 有效即保持唤醒，与电池是否正在充电无关。电池充满后仍保持联网。
- 电源状态读取失败时显示未知并暂时保持唤醒；成功读到电池供电后恢复正常休眠。

## 边界

检测在运行期间进行。设备已经深睡时，插入 USB 不会自动唤醒，需要短按 KEY 或等待原计划定时唤醒。电脑休眠、USB 挂起或使用仅充电线时，VBUS 仍有效即可保持联网。

深睡唤醒会输出上次睡眠前的供电状态。配置文件格式和 SD 照片不变。

## 0.15.0 离线报时

整点/半点、音量试听、多段暂停、准备与深睡协调，以及音频安装API见[声音与报时](announcement.md)。报时不刷新墨水屏；自动轮播和校时在保护窗口内延后。
