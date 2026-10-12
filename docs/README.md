# 文档目录

[项目首页](../README.md)

当前说明对应仓库版本0.14.35。日常操作、接口参考、规划和历史记录分别维护。

## 日常使用

建议阅读顺序：**使用指南 → 照片格式 → 配网与供电**。

| 文档 | 内容 |
|---|---|
| [使用指南](user-guide.md) | 准备SD、配网、照片维护、播放设置、按键和常见问题 |
| [照片格式](image-format.md) | 尺寸、六色、BMP约定和电脑端转换工具 |
| [Wi-Fi 配网](wifi-provisioning.md) | 开放热点、二维码、成功页和热点关闭 |
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
| [报时与活动检测设计](design/announcement-and-activity.md) | 尚未实现的音频、静音及声音活动方案 |
| [历史记录](history/README.md) | 早期M1、界面演进、HTTP演进和逐版本验收 |

历史页保留当时的配置和行为。当前操作以本目录中的使用与接口文档为准。

## 开源与贡献

[MIT许可证](../LICENSE) · [第三方许可](../THIRD_PARTY_NOTICES.md) · [贡献说明](../CONTRIBUTING.md) · [安全说明](../SECURITY.md)
