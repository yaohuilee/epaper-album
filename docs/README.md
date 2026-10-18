# 文档目录

[项目首页](../README.md)

当前说明对应源码版本0.17.7。日常操作、接口参考、规划和历史记录分别维护。

## 日常使用

建议阅读顺序：**使用指南 → 照片格式 → 配网与供电**。

| 文档 | 内容 |
|---|---|
| [使用指南](user-guide.md) | 准备SD、配网、照片维护、播放设置与夜间暂停、按键和常见问题 |
| [照片格式](image-format.md) | 上传前构图、相框横竖摆放、JPEG/PNG/BMP、六色和电脑端工具 |
| [Wi-Fi 配网](wifi-provisioning.md) | 开放热点、二维码、成功页和热点关闭 |
| [声音与报时](announcement.md) | ESP-TTS小乐、整点/半点、音量试听、多段暂停及报时API |
| [ESP-TTS构建](esp-tts.md) | 设备即时中文合成、Flash音库、构建和来源 |
| [供电与休眠](usb-power.md) | 电脑/充电器/电池运行策略及唤醒 |
| [联网校时](wifi-time.md) | NTP、RTC时间基准、网页设置及可选凭据导入 |
| [Web 控制台](web-layout.md) | 页面分工、设置弹窗、预览和时间显示 |

## 接口与开发

| 文档 | 内容 |
|---|---|
| [配置说明](configuration.md) | 全局配置、相册清单、播放规则和持久恢复 |
| [HTTP 状态与控制](http-status.md) | 状态字段、控制令牌、异步任务、播放/Wi-Fi/校时接口 |
| [SD 照片 API](http-files.md) | 列表、上传、下载、删除、创建/删除相册和网络工具 |
| [Windows 开发与安装](development.md) | ESP-IDF环境、构建、桌面测试、首次迁移与升级 |
| [硬件摘要](hardware.md) | 芯片、引脚、实测结果与未确认的硬件指标 |
| [运行架构](architecture.md) | 实际代码模块、调度、配置和状态保存 |
| [性能与压力测试](performance-stress.md) | 任务CPU、堆峰值、传输/换图并发和网络故障实测 |

## 图片制作示例

| 文档 | 内容 |
|---|---|
| [全球风景相册](world-album-workflow.md) | Qwen Image 2.1、100张图片和完整网络替换流程 |
| [早期60张制作记录](comfyui-album.md) | DreamShaperXL/niji工作流与当时的SD复制方式 |

## 验证与规划

| 文档 | 内容 |
|---|---|
| [验证摘要](validation.md) | 当前已验证范围、最近全流程结果和待测项目 |
| [后续计划](roadmap.md) | 未实现功能及后续验收工作 |
| [整点与半点报时设计](design/clock-announcement.md) | 确认设计：滴声、小乐与温湿度、多段暂停、音量试听、离线合成和调度 |
| [早期报时与活动检测规划](design/announcement-and-activity.md) | 温湿度和声音活动的早期规划背景 |
| [历史记录](history/README.md) | 早期M1、界面演进、HTTP演进和逐版本验收 |
| [照片构图与上传调整](history/photo-import-composition.md) | 0.17.4–0.17.7的处理流程、方向映射、页面命名与验收边界 |

历史页保留当时的配置和行为。当前操作以本目录中的使用与接口文档为准。

## 开源与贡献

[MIT许可证](../LICENSE) · [第三方许可](../THIRD_PARTY_NOTICES.md) · [贡献说明](../CONTRIBUTING.md) · [安全说明](../SECURITY.md)
