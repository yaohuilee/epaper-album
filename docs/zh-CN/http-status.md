# HTTP 状态与控制

[English](../http-status.md) | 简体中文


[文档首页](README.md) · [项目首页](../../README.zh-CN.md)

本文描述当前状态接口、播放控制、Wi-Fi及校时设置协议。网页操作见[使用指南](user-guide.md)，照片文件与相册创建/删除见[SD照片API](http-files.md)。

## 地址与运行条件

设备连接局域网后在端口80提供服务，地址由路由器分配。串口输出 `Status page: http://设备IP/`，配网成功页也显示该地址。

电脑、充电器或充电宝供电时保持Wi-Fi/HTTP在线。电池深睡时页面不可访问。首次没有有效Wi-Fi配置或手动请求时进入[离线配网](wifi-provisioning.md)，配网地址为 `http://192.168.7.60/setup`。

| 路径 | 用途 |
|---|---|
| `GET /` | 状态与设置页面 |
| `GET /albums` | 照片管理页面 |
| `GET /setup` | 配网页面 |
| `GET /api/status` | 只读设备状态快照 |

## 控制请求约定

先读取 `/api/status` 的 `control.token`。控制POST带 `X-Album-Token` 和 `Content-Type: application/json`，请求体上限4096字节。令牌每次启动变化，不是用户登录认证；设备不开放跨域读取，接口用于局域网。

异步命令返回202及 `job_id`，只代表已接受。轮询状态的 `control.job_id/state/error/busy`，匹配本次任务并确认 `succeeded` 或 `failed`。状态为queued/running时尚未完成，设备只保留最新命令结果，重启清空。

格式错误通常返回400/415，令牌错误403，任务忙或功能不可用409。设置任务的语义或存储错误可能在202之后通过failed报告。响应未确认时先查状态，不直接重发写入。

## 播放控制

| 路径 | 请求体 | 行为 |
|---|---|---|
| `POST /api/next` | `{}` | 按设备播放策略下一张 |
| `POST /api/settings` | 下方示例 | 保存相册选择、停留间隔或夜间暂停 |

```json
{"schema_version":1,"album_ids":["family","travel"],"interval_seconds":900}
```

至少提供 `album_ids`、`interval_seconds`、`night_pause` 中的一项。不允许空相册列表、未知/重复ID或非法间隔。

夜间暂停可单独提交，不改变相册选择、停留间隔或当前换图截止时间：

```json
{"schema_version":1,"night_pause":{"enabled":true,"start":"22:00","end":"06:00"}}
```

时间按设备显示时间的24小时 `HH:mm` 表示：开始早于结束为当天时段，晚于结束为跨午夜，两者不能相同。`enabled`、`start`、`end` 均必填。网页每15分钟一档，API仍接受合法的分钟值，旧配置不取整。缺省关闭，预设22:00–06:00。设置保存在相册清单的 `playback.night_pause`；状态接口返回 `album.night_pause` 和 `album.night_paused`，`album.next_seconds` 已计入暂停时间。暂停时段包含开始时刻，不包含结束时刻；手动下一张不受限制。

- 只提交相册选择：保留默认及各相册间隔。
- 只提交间隔：更新默认及已启用相册间隔，保留停用相册的间隔。
- 间隔范围120–604800秒。手动换图成功后从刷新完成重新计时。

保存使用配置校验、临时文件/备份替换和索引重建。若设置已保存但显示或NVS更新失败，错误说明会指出已保存，不能按完全未修改处理。

## 校时控制

| 路径 | 请求体 | 行为 |
|---|---|---|
| `POST /api/time-sync` | `{}` | 立即按当前服务器配置校时 |
| `POST /api/time-settings` | 下方示例 | 单独保存校时设置 |

```json
{"schema_version":1,"servers":["ntp.aliyun.com","ntp1.aliyun.com"],"interval_seconds":86400,"retry_seconds":900}
```

服务器、周期和重试间隔必须同时提交。服务器1–3个，每个1–127字符，为域名或IPv4地址；不接受URL、端口、重复或空服务器。周期3600–604800秒，重试60–86400秒。拒绝未知字段、重复键和非整数值。

校时停用时拒绝这些请求。成功校时需要NTP回复、RTC写入/读回验证及持久化完成。成功后按周期安排，失败后按重试间隔安排。保存设置从当前时刻开始新的周期，不立即校时。

保存只修改根配置 `time.sync` 的三个字段，保留凭据、超时及其他配置。相册剩余时长在系统时间跳变时保持。详见[联网校时](wifi-time.md)。

## Wi-Fi 设置

`POST /api/wifi-settings`：

```json
{"schema_version":1,"ssid":"YOUR_SSID","password":"YOUR_PASSWORD"}
```

SSID为1–32字节，区分大小写，设备使用2.4 GHz网络。密码省略表示保留，空字符串表示开放网络；新密码为8–63字节或64位十六进制PSK。只接受schema_version/ssid/password，拒绝重复键及非法类型。

配置写入SD根配置后重连，新IP可能变化。202不代表连接成功；失败时尝试恢复原凭据和连接。无法恢复时可重新进入配网或用读卡器修正配置。原密码不通过状态接口返回。

可见网络扫描、配网入口和成功结果见[SD/API参考中的扫描接口](http-files.md#wi-fi-可见网络扫描01421)及[配网说明](wifi-provisioning.md)。

## 状态数据

状态由应用任务采样，HTTP读取受锁保护的快照。刷屏或校时期间可能保留上一份数据，读取失败显示不可用。

| 对象 | 主要内容 |
|---|---|
| `device` | 版本、MAC、Flash、内部RAM/PSRAM堆统计、运行时间 |
| `network` | IP、Wi-Fi、USB主机、配网状态、NTP配置和结果 |
| `rtc` | 芯片、有效性、UTC基准和日历 |
| `environment` | SHTC3温湿度及读取状态 |
| `power` | AXP2101、VBUS、电池存在/电压和充电阶段 |
| `album` | 相册/照片数量、当前照片、间隔和下次换图；`image_width/image_height`为当前导入目标尺寸（0.17.4），`image_rotation_degrees`为固件解码旋转（0.17.5） |
| `control` | 令牌、命令结果、忙状态和文件索引修订 |

`network.last_sync_epoch/last_attempt_epoch` 为UTC秒，0表示无可信记录。最近成功时间及服务器持久化；最近尝试与错误属于本次启动。

成功响应的 `X-Status-Age-Ms` 为快照年龄毫秒。页面结合RTC快照、快照年龄及单调经过时间更新设备时钟，不使用电脑系统时间补齐无效RTC。

内部SRAM硬件容量512 KiB，RTC SRAM16 KiB；硬件容量不等于可用堆。内部堆/PSRAM分别提供总量、剩余、低水位和最大连续块。剩余之和不保证能分配同样大小的连续内存。充电阶段不等于实测电流或电量百分比。

历次接口/页面变更的完整记录见[HTTP历史说明](history/http-status-changes.md)。

## 声音与报时

报时与试听统一使用ESP-TTS小乐；POST /api/announcement-settings、/api/announcement-pause-settings分别保存设置，/api/announcement-test接受kind=hour/half及volume_percent。省略engine即可使用小乐，显式指定时只接受esp-tts。实际完成由completed_test_id及last_test_result确认，不以控制job入队成功代替播报完成。详见[报时说明](announcement.md)。

录音包上传、下载和激活接口已移除。CONFIG_EPAPER_DIAGNOSTICS启用时提供带控制令牌的/api/diagnostics统计入口；常规构建默认关闭。限时网络故障仅允许USB主机连接时注入，测量字段和限制见[性能压力测试](performance-stress.md)。
