# 第三方组件与资源

本项目原创代码和文档采用[MIT](LICENSE)。下列第三方代码或字体数据保留原有许可，不由项目MIT覆盖。

| 内容 | 位置/使用方式 | 许可 |
|---|---|---|
| Project Nayuki QR Code generator | components/services/qrcodegen.c、qrcodegen.h，保留原源码声明 | [MIT](third_party/licenses/Nayuki-QR-MIT.txt) |
| Noto Sans SC | 配网中文固定文字的位图来源；原字体仅存本机.local | [OFL-1.1](third_party/licenses/NotoSansSC-OFL.txt) |
| Noto Sans Mono | 配网ASCII位图字模来源；原字体仅存本机.local | [OFL-1.1](third_party/licenses/NotoSansMono-OFL.txt) |
| ES8311初始化参数 | components/devices/audio_output.c，参考[Espressif esp-bsp](https://github.com/espressif/esp-bsp/blob/master/components/es8311/es8311.c)，保留来源与版权 | [Apache-2.0](third_party/licenses/ESP-IDF-Apache-2.0.txt) |
| ESP-TTS / ESP-SR | 通过安装器获取S3预编译库与小乐音库；本地依赖不随源码仓库分发 | [ESPRESSIF MIT](third_party/licenses/ESP-TTS-Espressif-MIT.txt)，许可限Espressif产品使用；API头文件保留其Apache-2.0声明 |
| ESP-IDF v5.5.3 | 外部SDK依赖，未复制SDK到仓库 | [Apache-2.0主许可](third_party/licenses/ESP-IDF-Apache-2.0.txt)，SDK内第三方组件另有各自许可 |
| Pillow | 电脑端图片/字模工具的外部依赖 | [MIT-CMU许可](https://github.com/python-pillow/Pillow/blob/11.2.1/LICENSE) |

二维码来源：[Project Nayuki](https://www.nayuki.io/page/qr-code-generator-library)。字体来源：[Noto Sans SC](https://github.com/google/fonts/tree/main/ofl/notosanssc)、[Noto Sans Mono](https://github.com/google/fonts/tree/main/ofl/notosansmono)。

## 配网字模

`components/services/provision_font.h` 中的派生字模命名为AlbumProvisionGlyphs，保留OFL-1.1许可及版权。原TTF未随仓库分发；固件编译直接使用已提交头文件。

需要重生成时，安装Pillow，将上述两个OFL字体下载到 `.local/fonts/NotoSansSC.ttf` 和 `.local/fonts/NotoSansMono.ttf`，执行：

```powershell
python tools/provision_font.py
```

也可用 `--cjk-font` 和 `--ascii-font` 指定对应字体位置。生成器代码为MIT，输出字体数据为OFL-1.1。修改或分发派生字模须保留OFL许可和版权，不使用原字体的保留名称作为派生字体名称。

## SDK与模型

ESP-IDF内部的cJSON、lwIP等组件按各自文件许可使用。发布包含SDK或其依赖的固件时，应随分发保留相应许可/NOTICE要求。本仓库不发布带私人Wi-Fi数据的构建产物。

图片生成工作流不包含模型权重；所用模型、生成素材和第三方服务分别遵循其自身许可与条款。

## ESP-TTS小乐

0.16.0起统一使用乐鑫官方ESP-TTS及小乐音库，库、音库和头文件从固定官方源码树下载。库与音库采用上游ESPRESSIF MIT许可，授权条件包含仅用于Espressif产品；本项目的ESP32-S3属于该使用范围。API头文件中的Apache-2.0声明仍保留。安装器下载完整LICENSE，分发包含这些第三方资源的固件时也应保留对应许可声明。

本项目MIT许可证只覆盖原创代码与文档，不将第三方库或音库改为无使用范围限制的MIT。SDK二进制和音库保存在.local且不提交。此前Qwen生成工具和录音包方案已停用；历史试验说明不作为现行资源分发授权。
