# 已结束的ESP-TTS试听试验（0.15.3）

[English](../../history/esp-tts-trial.md) | 简体中文


现行构建与操作见[ESP-TTS小乐](../esp-tts.md)，本页保留当时的对比记录。

# ESP-TTS 离线合成试验

[项目首页](../../../README.zh-CN.md) · [声音与报时](../announcement.md)

0.15.3增加独立的ESP-TTS试听。设备根据文字在本机合成语音，不需要网络或SD卡WAV。自动报时仍使用原语音包，试听不会改变保存的音量、照片和报时设置。

## 真机结果

整点、半点试听的音频准备分别耗时3593毫秒、3833毫秒，包含各段合成、重采样和序列组装。两次实际播放结果均为`succeeded`，用户确认完整播报清晰可听。试听后内部可用堆约151 KiB、PSRAM可用约7.74 MiB，照片和自动报时去重记录保持不变。这是两次短期测试，未进行长时间压力测试。

初次试验使用官方小鑫精简音库`esp_tts_voice_data_xiaoxin_small.dat`，2913777字节。语速档位2（官方范围0–5），输出16 kHz、单声道、16位PCM，经线性插值转换到现有播放器的24 kHz。整点5声、半点3声、末声差异及三处1秒停顿沿用现有音频序列逻辑。

随后以音量40%、语速档位2依次比较三套官方音库，实际播放均成功，照片保持不变：

| 编号 | 音库 | 音库字节数 | 音频准备耗时 |
|---|---|---:|---:|
| A | 小鑫精简版 | 2913777 | 3623毫秒 |
| B | 小乐 | 2938039 | 3993毫秒 |
| C | 小鑫完整版 | 3821311 | 3603毫秒 |

用户选择B“小乐”，设备试验音库已切回小乐，后续本机烧录也使用该选择。完整版和精简版属于同一小鑫声线的不同资源版本，文件大小不代表明亮度或音质高低。

## 使用

打开“声音与报时”的报时设置弹窗，点击“ESP-TTS 试听”。使用当前音量草稿；播放完成后显示实际结果。原“试听整点”“试听半点”仍播放原WAV语音包。

API：`POST /api/announcement-test`，需要当前页面的`X-Album-Token`，例如：

```json
{"kind":"hour","engine":"esp-tts","volume_percent":40}
```

`kind`支持`hour`和`half`。省略`engine`或指定`wav`仍使用原语音包。状态中的`tts_available`表示音库可用，`tts_test_available`表示当前允许试播；`last_test_engine`和`test_synthesis_ms`记录最近试听使用的引擎和音频准备耗时。实际播放完成仍通过`completed_test_id`与`last_test_result`判断。

`tts_voice`显示当前Flash音库的标识：`xiaole`、`xiaoxin-small`或`xiaoxin`。小鑫完整版与custom变体具有相同头部标识，不通过该字段区分。

## 构建

```powershell
./tools/Install-EspTts.ps1
./tools/Select-EspTtsVoice.ps1 -Voice xiaole
# 在ESP-IDF menuconfig中启用 Epaper album / Enable experimental offline ESP-TTS preview
./tools/Build.ps1
```

默认关闭`CONFIG_EPAPER_ESP_TTS`。安装器从官方ESP-SR固定源码树`78a923d29a45e4e1433b909d3da12ab5b0f994fc`下载S3库、头文件、音库和许可证，保存在被忽略的`.local/esp_tts_sdk/`。构建启用时，`idf.py -p COM5 flash`会一并写入音库。

选择工具支持`xiaole`、`xiaoxin-small`、`xiaoxin`、`xiaoxin-custom`。缺失音库从同一官方版本下载；选择后校验大小和头部，保存音库到`.local/tts-selected.dat`并记录SHA-256。没有选择文件时，构建使用小鑫精简版。改变选择后重新烧录才会影响设备。

`voice_data`分区位于`0x600000`，容量4 MiB；已有应用、NVS和`album_state`地址不变。音库头部检查失败时不会创建合成器，页面不显示试听入口。

## 声音调整与来源

语速可调，声线主要由官方音库决定。本次没有提供文字描述声线、真人克隆或音高调整。当前“小乐”试听不替代原女中音包。

官方资料：[ESP-TTS说明](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/speech_synthesis/readme.html)、[ESP-SR源码及音库](https://github.com/espressif/esp-sr/tree/master/esp-tts)。第三方库、头文件和音库保留上游许可证，项目MIT许可证不重新授权这些资源。安装器保留上游`LICENSE`及头文件中的许可声明。
