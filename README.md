# epaper-album

基于 ESP32-S3 PhotoPainter 六色墨水屏的离线相册。照片保存在 SD 卡，通过按键或局域网 Web 页面管理和轮播。播放使用本地照片和 RTC，不依赖持续联网；已配网设备连接失败时继续轮播，不会自动进入配网。

**当前源码版本：0.17.0。** 目标硬件为 ESP32-S3、16 MB Flash、8 MB PSRAM、800×480 六色墨水屏。开发环境为 Windows 原生 ESP-IDF v5.5.3。

## 从这里开始

| 想做什么 | 阅读入口 |
|---|---|
| 配网、打开页面、上传照片和设置轮播 | [使用指南](docs/user-guide.md) |
| 查询照片尺寸、颜色和转换方法 | [照片格式](docs/image-format.md) |
| 查找全部说明 | [文档目录](docs/README.md) |
| 在 Windows 构建或更新固件 | [开发与安装](docs/development.md) |
| 查看已验证范围和未完成事项 | [验证摘要](docs/validation.md) · [后续计划](docs/roadmap.md) |

## 支持的硬件

本项目面向微雪电子（Waveshare）的 **ESP32-S3-PhotoPainter** 开发，已在该型号实物设备上验证运行。实测与官方规格的对应关系、引脚和验证范围见[硬件说明](docs/hardware.md)；测试设备的具体 PCB 修订版本尚未确认。

| 项目 | 目标硬件 |
|---|---|
| 主控与存储 | ESP32-S3，16 MB Flash，8 MB PSRAM |
| 显示屏 | 7.3 英寸 E Ink Spectra 6（E6），800×480，黑、白、红、黄、蓝、绿六色 |
| 存储与接口 | microSD 卡槽、USB Type-C、BOOT / KEY / PWR 三个按键 |
| 时钟与环境 | PCF85063 系列 RTC、SHTC3 温湿度传感器 |
| 音频 | 双麦克风、扬声器接口、ES7210 / ES8311 音频芯片 |

### 官方资料

- [产品页面](https://www.waveshare.com/product/displays/e-paper/esp32-s3-photopainter.htm)
- [中文产品文档](https://docs.waveshare.net/ESP32-S3-PhotoPainter/) · [英文 Wiki](https://www.waveshare.com/wiki/ESP32-S3-PhotoPainter)
- [板级原理图（PDF）](https://files.waveshare.com/wiki/ESP32-S3-PhotoPainter/ESP32-S3-PhotoPainter-Schematic.pdf)
- [厂商示例代码与固件仓库](https://github.com/waveshareteam/ESP32-S3-PhotoPainter)

以上链接用于查阅硬件和厂商资料。本仓库不分发厂商固件；本项目的配置文件、照片格式和操作方法见本仓库文档。

### 第三方固件说明

本项目是面向 Waveshare ESP32-S3-PhotoPainter 硬件独立开发的第三方固件，与 Waveshare 无隶属关系，也未经其官方背书。Waveshare 和产品名称用于标识兼容硬件。项目原创代码与文档采用 [MIT 许可证](LICENSE)，第三方代码与资源的来源及许可证见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

### 实物照片

| 正面 | 背面与接口 |
|---|---|
| 待补充：相框、屏幕与显示效果 | 待补充：BOOT / KEY / PWR、SD 卡槽与 USB 接口 |

实物照片暂未上传，图片授权信息将在照片补充时一并标注。

## 已实现的功能

- **离线相册**：SD 子目录分组，启用相册选择、顺序/随机播放、逐相册/轮流播放、定时换图和断点恢复。
- **夜间暂停**：可设置设备时间的自动轮播暂停时段，预设22:00–06:00，支持跨午夜；手动换图仍可用。
- **Web 控制台**：设备概览、网络与时间、环境与供电、播放设置和文件夹式照片库。
- **网络维护**：照片预览、上传、下载和删除；API 创建相册、删除空相册及完整替换照片库。
- **Wi-Fi 配网**：开放热点、墨水屏双二维码、手机配网页面、可见网络扫描和隐藏网络输入。
- **RTC 校时**：周期 NTP、失败重试、手工同步，以及网页保存 Wi-Fi 和校时设置。
- **离线报时**：整点/半点滴声、ESP-TTS小乐时间及温湿度、音量试听、独立多段暂停和设备离线合成。见[报时说明](docs/announcement.md)。
- **供电管理**：电脑、充电器或充电宝供电时保持联网；仅电池供电时按计划深睡。

声音活动检测和低电量自动策略**尚未实现**。温湿度读取与网页展示已实现。

## 日常使用

从源码构建前先执行`tools/Install-EspTts.ps1`安装小乐依赖；参见[构建说明](docs/esp-tts.md)。

1. 设备需要已安装本固件，并装有有效配置和相册清单的 SD 卡。首次准备见[使用指南](docs/user-guide.md)。
2. 无有效 Wi-Fi 时连接墨水屏显示的开放热点，访问 `http://192.168.7.60/setup`。保存成功后热点保留30秒。
3. 切回家庭 Wi-Fi，打开成功页显示的设备地址。家庭网络地址由路由器分配，不使用固定配网地址。
4. 在“照片库”打开相册并上传 JPEG 或24位 BMP，在“播放设置”启用相册并保存停留时间。

照片库上传普通 JPEG 或24位无压缩 BMP 后，设备自动裁剪、缩放并转换为六色抖动 BMP，页面显示进度。单文件上限16 MiB；渐进式 JPEG、CMYK、PNG 等可先用[电脑端工具](docs/image-format.md)处理。

## 按键与供电

| 按键 | 短按 | 长按 |
|---|---|---|
| BOOT | 唤醒并切换下一张 | 约2秒请求深睡；外部供电时忽略 |
| KEY | 唤醒/输出诊断，保留当前照片 | 约2秒后松开，进入配网 |
| PWR | 关机状态下开机 | 约6秒硬件关机 |

已进入深睡时，插入 USB 不会立即唤醒；短按 KEY 或等待计划唤醒。电池深睡期间 Web 页面不可访问。详见[供电与休眠](docs/usb-power.md)。

## SD 卡与照片

```text
/
├── config.json          # Wi-Fi、校时、显示等全局配置
└── albums/
    ├── albums.json      # 相册选择、播放顺序和间隔
    ├── family/
    │   └── 001.bmp
    └── travel/
        └── 001.bmp
```

仓库的 [sdcard/](sdcard/) 提供配置模板，不含照片。默认模板只启用 `family`，停留时间15分钟，Wi-Fi 初始关闭。完整字段和恢复规则见[配置说明](docs/configuration.md)。

已完成的制作示例：[Qwen Image 2.1 全球风景相册与网络替换](docs/world-album-workflow.md)。

## 代码与数据

`main/` 是启动与调度入口，`components/` 包含硬件、相册和网络服务，`tools/` 提供构建、测试及照片处理工具，`tests/` 保存桌面测试。模块说明见[运行架构](docs/architecture.md)。

仓库保存源码、配置模板、测试、文档和工作流。原机固件、完整 Flash 备份、私人照片、录音、生成包、构建输出、Wi-Fi 凭据和账号令牌保留在本地。第三方许可声明保留在对应源码和许可文件中。

## 许可证

本项目原创代码和文档采用[MIT许可证](LICENSE)，允许使用、修改、分发和商业使用，须保留版权及许可声明。

第三方组件及资源保留各自许可证。二维码库为MIT；配网位图字模为OFL-1.1；ESP-IDF及其组件按SDK随附许可证使用。具体来源、许可文本和字模重生成方法见[第三方声明](THIRD_PARTY_NOTICES.md)。本项目基于PhotoPainter硬件独立开发，复用的第三方代码和资源列于上述声明。
