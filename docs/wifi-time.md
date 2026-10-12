# Wi-Fi 与 RTC 校时

[文档首页](README.md) · [项目首页](../README.md)

当前支持周期NTP、失败重试、手工同步，以及网页保存Wi-Fi和校时设置。设备RTC保存UTC，网页显示北京时间。日常配网见[配网说明](wifi-provisioning.md)，接口见[HTTP参考](http-status.md)。

## 调度与供电

完整开机按计划尝试校时，默认成功后每24小时同步，失败15分钟后重试。普通换图或KEY深睡唤醒不重置校时计划。

电脑、充电器或充电宝供电时保持Wi-Fi/HTTP在线，校时共用连接。仅电池供电时任务结束关闭网络并按下一次换图/校时事件深睡。具体条件见[供电说明](usb-power.md)。

默认大陆服务器为ntp.aliyun.com、ntp1.aliyun.com、ntp2.aliyun.com，按配置顺序尝试；默认连接超时15秒，校时超时20秒。配置可修改，连接失败不停止相册。

## 网页操作

“网络与时间”的Wi-Fi卡片用于保存SSID/密码并重连；RTC与网络校时卡片提供校时设置和立即同步。

保存校时设置从当前时刻开始新周期，不立即同步。手工同步成功后按正常周期安排，失败按重试间隔安排。最近成功时间/服务器持久化，最近尝试和错误仅属于本次启动。

## 可选：构建时导入本机凭据

`WIFI.txt` 使用 UTF-8：第一行为 SSID，第二行为密码。不去除行内空格。可使用空密码的开放网络；一般密码为 8–63 字节，64 字节必须是十六进制 PSK。

在 Windows 原生 IDF 环境中执行：

```powershell
python tools/import_wifi.py C:/private/WIFI.txt
idf.py menuconfig
# 启用 EPAPER_PROVISION_WIFI，然后保存退出
./tools/Build.ps1
```

脚本生成 `.local/wifi_credentials.h` 和 `.local/wifi-provision.json`，均被 Git 忽略。默认仓库构建不启用内置凭据；只有本机明确启用后才编译私人设置。固件和 SD 配置含连接数据，也应留在本地。

本机专用固件第一次启动时读取并验证 SD 的 `/config.json` 和相册清单，合并 `network` 和 `time.sync`，保留相册、显示及其他设置。先写临时文件并同步，再将原配置重命名为 `/config.json.pre-wifi`，最后安装新配置。断电中断安装时尝试恢复原文件。成功标记保存在 `album_state`，相同内置设置只导入一次，此后允许直接编辑 SD 配置，不会在每次开机覆盖。

已有 `.pre-wifi` 备份且新的内置设置与现有文件不同，则拒绝覆盖；更改 Wi-Fi 时可直接编辑当前配置，或先在读卡器上归档旧备份再重新导入。SD 不可写或配置无效时不安装新设置，继续使用已有有效相册配置或进入恢复模式。

## SD 配置字段

下面的连接信息为占位示例，需要替换：

```json
{
  "network": {
    "wifi_enabled": true,
    "ssid": "YOUR_SSID",
    "password": "YOUR_PASSWORD"
  },
  "time": {
    "rtc_storage": "utc",
    "sync": {
      "mode": "ntp",
      "servers": ["ntp.aliyun.com", "ntp1.aliyun.com", "ntp2.aliyun.com"],
      "interval_seconds": 86400,
      "retry_seconds": 900,
      "connect_timeout_seconds": 15,
      "sync_timeout_seconds": 20
    }
  }
}
```

以上是需要合并到全局配置的字段，单独使用不能替代包含 `schema_version`、`album` 的完整文件。启用时必须同时设置 `wifi_enabled: true` 和 `mode: "ntp"`。停用时设为 `false` 和 `"manual"`。

校时间隔支持 3600–604800 秒，重试间隔 60–86400 秒，连接及校时超时各 5–60 秒；服务器 1–3 个，长度不超过 127 字节。

## RTC 与相册计时

NTP 返回的时间写入 PCF85063 的 UTC 日历，停振标志清零，恢复运行后立即读回核对（允许 0–2 秒差）。只有读回一致且持久化成功，才设置 UTC 迁移标记。诊断中的当地时间按北京时间 UTC+8 显示。

原设备的 RTC 曾写入北京时间；首次成功校时前明确标记为未迁移，不直接按 UTC 解释。失败不会伪造迁移成功。时间跳变时同步调整照片和刷新重试的截止时间，保持剩余停留时长。

更新 0.2.0 相册固件时保留 `album_state`（`0x500000`–`0x51ffff`）；不得重新初始化或全片擦除。网络使用 RAM 存储，不依赖旧固件的默认 NVS Wi-Fi 数据。

此版本实现 RTC 校时；整点/半点语音播报仍属于后续阶段。
