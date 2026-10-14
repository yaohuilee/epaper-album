# 声音与报时

[文档目录](README.md) · [技术设计](design/clock-announcement.md) · [ESP-TTS构建](esp-tts.md)

从0.16.0起，自动报时和整点、半点试听统一由设备离线运行ESP-TTS，小乐音库、语速档位2。语音根据当前时间和传感器数据即时合成；不再读取SD卡录音包，也不依赖Wi-Fi。

## 播放规则

整点5声，半点3声，每秒一声；普通滴声1000 Hz、100毫秒，末声1500 Hz、250毫秒。末声开始落在目标时刻T：整点从T−4秒开始，半点从T−2秒开始。

末声结束后静默1秒，说“现在时刻”；结束后静默1秒，说“X点整”或“X点三十分”；结束后静默1秒，再说温度、湿度。温度最多1位小数，省略零小数及负零；湿度四舍五入为整数。

“声音与报时”页面设置整点、半点开关、音量及最多8个独立暂停时段。时段支持同日或跨午夜，按目标报时点判断，包含开始、不包含结束。22:00–06:00暂停允许06:00报时从05:59:56开始滴声。

试听使用音量草稿，不保存设置，不受暂停时段限制。临近自动报时或正在播放时不能试听。页面等待实际完成结果；仅时间播报成功时明确显示温湿度未完成。

## 配置和迁移

根配置的announcement保留enabled、minutes（0/30）、volume_percent、include_temperature、include_humidity、temperature_decimals（0/1）和pause_periods。引擎固定为engine=esp-tts、voice=xiaole、speech_speed=2。

启动时将旧voice_pack、audio_root及旧quiet_hours配置迁移为现行配置，使用原有SD原子写入及配置缓存机制。Wi-Fi、相册、音量和暂停时段保留。写入失败时不启用报时并报告配置错误。已有SD录音文件不会被删除，但不再使用。

## 调度与故障处理

设备以外部RTC恢复的时间调度，RTC保存UTC。电池模式在目标前30秒完成最后一次计划唤醒，目标前20秒开始准备：读取并校验RTC、重新测量温湿度、合成完整PCM序列，随后使用RTC秒跳变锚定末滴时刻。合成每句限5秒、整段准备限10秒、音频总长限40秒；准备超时不播放过期报时。

语音在RAM中准备，16 kHz转换到24 kHz后，由ES8311播放器播放；播放完释放缓冲区、合成器和Flash映射。正在准备/播放时暂停换图及长任务，下一张请求合并排队；屏幕不显示临时报时通知。

传感器异常播报不可用提示。温湿度合成失败可降级为时间播报；时间合成失败停止该次播放。Flash小乐音库不可用时，不启用自动播报。自动输出前持久保存NVS去重记录，防止重启重复；不补播已错过时刻。

## API

| 接口 | 内容 |
|---|---|
| POST /api/announcement-settings | 保存整点/半点、音量开关 |
| POST /api/announcement-pause-settings | 单独保存暂停时段 |
| POST /api/announcement-test | kind=hour/half、volume_percent=1–100；可省略engine，指定时仅接受esp-tts |

写操作仍需要当前页面的X-Album-Token。状态显示engine、voice、speech_speed、ready、phase、next_target_epoch、last_result、test_request_id、completed_test_id和test_synthesis_ms。job成功仅表示请求入队；completed_test_id及last_test_result才表示播放结束。

录音包上传、下载、激活API已移除。音库通过固件构建和烧录安装在Flash，具体来源、许可及步骤见[ESP-TTS](esp-tts.md)。绝对起音误差、声压和长期功耗仍需仪器测量。
