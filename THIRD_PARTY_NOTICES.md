# 第三方组件与资源

本项目原创代码和文档采用[MIT](LICENSE)。下列第三方代码或字体数据保留原有许可，不由项目MIT覆盖。

| 内容 | 位置/使用方式 | 许可 |
|---|---|---|
| Project Nayuki QR Code generator | components/services/qrcodegen.c、qrcodegen.h，保留原源码声明 | [MIT](third_party/licenses/Nayuki-QR-MIT.txt) |
| Noto Sans SC | 配网中文固定文字的位图来源；原字体仅存本机.local | [OFL-1.1](third_party/licenses/NotoSansSC-OFL.txt) |
| Noto Sans Mono | 配网ASCII位图字模来源；原字体仅存本机.local | [OFL-1.1](third_party/licenses/NotoSansMono-OFL.txt) |
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
