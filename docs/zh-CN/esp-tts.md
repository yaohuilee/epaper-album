# ESP-TTS小乐：构建和音库

[English](../esp-tts.md) | 简体中文


[项目首页](../../README.zh-CN.md) · [报时说明](announcement.md)

0.16.0统一使用ESP-TTS小乐，自动报时与试听均在设备合成。音库为官方esp_tts_voice_data_xiaole.dat（2938039字节），语速档位2，原始输出16 kHz、单声道、16位PCM。系统重采样为24 kHz并加入倒计时与静默。

## Windows构建

```powershell
./tools/Install-EspTts.ps1
./tools/Build.ps1
# 已加载ESP-IDF环境后执行
idf.py -p COM5 flash
```

安装器从官方固定源码树78a923d29a45e4e1433b909d3da12ab5b0f994fc下载S3库、头文件、小乐音库和许可证至.local/esp_tts_sdk/。本地依赖不提交，CONFIG_EPAPER_ESP_TTS默认开启；缺少依赖时构建提示先运行安装器。关闭该编译选项时相册仍可用，但语音不可用。

烧录自动包含voice_data分区（0x600000、4 MiB）。仅支持小乐头部标识；旧试验版的其他音库及tts-selected.dat不再作为构建输入。音库固定在Flash，不经SD或HTTP更新。

## 选择依据和许可

0.15.3曾按音量40%、语速2对比小鑫精简版、小乐、小鑫完整版，全部成功播放，用户选择小乐。0.16.0完整短句合成的整点、半点准备分别耗时4213、4342毫秒，用户确认播报正常。[现行验收](validation.md) · [历史对比](history/esp-tts-trial.md)。

官方资料：[ESP-TTS说明](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/speech_synthesis/readme.html)、[SDK及音库](https://github.com/espressif/esp-sr/tree/master/esp-tts)。第三方资源保留上游许可，安装器保存LICENSE及头文件许可声明；项目MIT许可证不重新授权这些资源。仓库不包含库二进制、音库、模型权重或私人录音。
