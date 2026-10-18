# Windows 原生开发

[English](../development.md) | 简体中文


[文档首页](README.md) · [项目首页](../../README.zh-CN.md)

固定 ESP-IDF v5.5.3，目标 ESP32-S3，16 MB Quad Flash / 8 MB Octal PSRAM。不需要 WSL 或 Docker。所有安装路径和项目路径应避免空格，SDK 与工具路径尽量短。

## 安装

可使用 [Espressif 官方 Windows 安装流程](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32s3/get-started/windows-setup.html)，选择 v5.5.3 与 ESP32-S3；也可用现有 Git/Python 创建原生工具环境：

```powershell
git clone --branch v5.5.3 --depth 1 --recursive --shallow-submodules https://github.com/espressif/esp-idf.git C:/Espressif/esp-idf-v5.5.3
$env:IDF_TOOLS_PATH = 'C:/Espressif/tools'
python C:/Espressif/esp-idf-v5.5.3/tools/idf_tools.py install --targets esp32s3
python C:/Espressif/esp-idf-v5.5.3/tools/idf_tools.py install-python-env
```

安装位置可自行指定，无需系统 PATH 永久修改。若本机配置了已经停止的 Git 代理，可对该次命令加 `git -c http.proxy= clone ...`，不要全局覆盖自己的网络配置。

## 环境激活与构建

在项目根目录新建 `.local/idf-env.json`（已被 Git 忽略），记录本机实际安装路径：

```json
{
  "idf_path": "C:/Espressif/esp-idf-v5.5.3",
  "tools_path": "C:/Espressif/tools",
  "python_directory": "C:/Python312"
}
```

```powershell
. ./tools/Enter-Idf.ps1
./tools/Install-EspTts.ps1
./tools/Build.ps1
```

也可直接传 `-IdfPath`、`-ToolsPath`、`-PythonDirectory`，不创建本机配置。脚本激活官方 `export.ps1`，构建脚本只编译，不自动烧录。

## 当前应用与测试

运行行为见[使用指南](user-guide.md)，早期迁移背景见[历史M1](history/m1-implementation.md)。应用挂载 SD、读取配置与相册清单、显示照片并安排下一次事件；仅电池供电时默认10秒空闲后深睡；电脑或USB外部供电保持联网。SD 不格式化。0.3.0 可将本机内置 Wi-Fi 设置一次性合并到全局配置，并通过 NTP 写入及验证 RTC UTC 日历；流程见 [联网校时](wifi-time.md)。

`./tools/Test.ps1` 自动寻找 Visual Studio C++ 编译器，并使用 SDK 中的 cJSON 源码编译Windows桌面测试程序。需要 Visual Studio C++ Build Tools；可用 `-VsDevCmd` 显式指定初始化脚本、`-IdfPath` 指定 SDK。

- `core_test.c`：日历、BCD、闰日、OS 标志与传感器 CRC。
- `album_test.c`：BMP 旋转与非法文件、配置、索引、两种策略、随机不重复与重启恢复。
- `runtime_test.c`：生产 runtime 代码配合模拟时钟/显示/NVS，验证间隔从完成时开始、手动计时、失败不提交、深睡/冷启动、中断刷新恢复和配置缓存。
- `provision_test.c`：配网画面边界、颜色与非法输入。
- `announcement_test.c`：报时边界、旧配置迁移、分配失败、短句合成失败清理及滴声/静默时序。
- `pcm_resample_test.c`：16 kHz到24 kHz插值、边界、极值和容量检查。
- `announcements_ui_test.cjs`：实际页面脚本的试听入队与完成结果；安装Node.js后由Test.ps1执行。

语音引擎为ESP-TTS小乐，默认启用。安装器下载固定版本SDK库与音库到.local，构建和烧录将小乐音库写入voice_data分区（0x600000，4 MiB）；无需SD录音包。来源与许可见[ESP-TTS构建](esp-tts.md)。

CONFIG_EPAPER_DIAGNOSTICS默认关闭。启用后可测量任务CPU和堆低水位，并在USB主机连接时进行限时网络故障注入；复测工具、实测数据和限制见[性能压力测试](performance-stress.md)。

测试生成物和模拟 SD 位于 `.local/`，不接触真实 SD。正式固件通过 `./tools/Build.ps1` 编译，脚本不自动烧录。

首次从原固件迁移，先以 MAC 确认目标并备份/校验完整 Flash，再初始化新状态分区与烧录，见下方首次安装说明。更新相册固件保留状态分区。构建和桌面测试不能替代屏幕、按键、供电与深睡的硬件验收；记录见 [验证结果](validation.md)。

凭据导入的桌面测试可单独执行 `python -X utf8 tests/import_wifi_test.py`；只使用临时合成数据，不读取真实 WIFI.txt。

## 首次迁移与常规升级

首次从原机固件迁移：先核对设备MAC，备份并校验完整16 MB Flash，确认分区布局，再初始化本项目的状态分区并烧录。`album_state` 位于0x500000，大小0x20000；原始迁移背景见[历史M1说明](history/m1-implementation.md#安装与数据)。

已经安装本项目后的常规升级保留 `album_state`，不重复初始化、不全片擦除。构建脚本不会自动烧录。在已激活IDF环境中使用实际端口：

```powershell
idf.py -p COM5 flash
```

COM5只是本机例子，使用前核对目标。SD配置和照片独立保留。

## 原图转换依赖

0.17.0 新增 espressif/esp_jpeg 1.3.1，IDF Component Manager 根据 components/services/idf_component.yml 与 dependencies.lock 下载依赖。首次构建需要网络，managed_components 和构建产物不提交。CONFIG_JD_USE_ROM=n 使用软件 TJpgDec，支持灰度图；启用 ROM 会在 CMake 配置阶段拒绝构建。转换与 JPEG 解码源使用 -O2。

components/core/image.c 是可在桌面测试的裁剪、方向、BMP解析和抖动核心；services/image_import.c 管理低优先级任务、SD暂存、进度及发布。运行 tools/Test.ps1 包含图像核心和上传页面异常测试。Enter-Idf.ps1 重复激活时沿用已启用的 IDF Python 虚拟环境。

0.17.2 新增官方 espressif/libpng 1.6.58~2，传递依赖 espressif/zlib 1.3.2~1，具体校验信息保存在 dependencies.lock。services/png_import.c 按行解码并限制库分配，内存位于PSRAM；core/image.c 在目标图像上累计双线性采样，支持Adam7实际像素筛选。无需整张来源RGBA缓冲。PNG解码、libpng和zlib使用-O2，桌面图像测试覆盖透明白底及交错一致性。

## 中英文维护

构建内置i18n.js，无额外前端运行依赖。tools/Test.ps1包含i18n_test.cjs；文档对应关系及本地链接由python -X utf8 tools/check_docs.py检查。默认docs为英文，docs/zh-CN保留中文，用户可见变更同步两种语言。
