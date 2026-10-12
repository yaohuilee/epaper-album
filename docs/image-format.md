# 照片格式

[文档首页](README.md) · [项目首页](../README.md)

当前只接收经过电脑端处理的 BMP，设备端不执行 JPEG/PNG 解码、缩放、调色或抖动。

- 横屏 800×480，竖屏 480×800；`rotation_degrees` 0/180 对应横屏，90/270 对应竖屏。不匹配则跳过。
- Windows BMP，BITMAPINFOHEADER 40 字节，planes=1、24 bpp、BI_RGB=0（无压缩）。
- 标准底向上正高度；当前不接收负高度、索引色、32 位透明、RLE 或位掩码压缩。
- 像素按 BGR 存储，每行补齐到 4 字节；解析以文件头偏移为准，检查偏移、长度、尺寸、行跨度及越界。
- 文件内颜色仅允许 RGB：黑 `000000`、白 `FFFFFF`、红 `FF0000`、黄 `FFFF00`、蓝 `0000FF`、绿 `00FF00`。遇到其他色值拒绝该图并记录诊断，不能默默当白色。
- 对应面板色码 0、1、3、2、5、6；两像素打包为一字节，800×480 缓冲区为 192000 字节。
- 常规 54 字节文件头加像素数据，约 1152054 字节；解析器不只靠文件大小判断合法性。

推荐流程：应用 EXIF 方向 → 转 sRGB 并合成透明背景 → 按比例裁剪或补白 → 高质量缩放到最终尺寸 → 亮度/对比调整 → 六色抖动 → 保存 24 位无压缩 BMP。照片可用误差扩散，文字/线条保持纯色；抖动后不再缩放或保存 JPEG。

六色编码是文件约定，不代表面板实测色度。最佳色彩需以后用面板反射色校准并上屏比较。当前直接显示整张照片，不叠加时钟/状态栏，以免破坏预处理图；设备状态通过Web页面或串口查看；配网使用专门画面。语音播报尚未实现。

## 电脑端处理工具

`tools/prepare_photos.py` 使用 Pillow，将 PNG/JPEG/WebP/BMP 转为可直接播放的格式：EXIF 方向、透明背景合成白色、按比例居中裁剪、Lanczos 缩放、轻微对比/锐化、固定六色 Floyd–Steinberg 误差扩散，最后保存 RGB 24 位 BMP。不会覆盖同名输出。

```powershell
python -m pip install Pillow
python tools/prepare_photos.py originals prepared --preview-dir previews
```

默认输出 800×480；竖屏加 `--rotation 90` 或 `--rotation 270`，同时在设备配置中使用对应旋转。工具只在量化前缩放；预览 PNG 和设备 BMP 使用相同像素。生成的 `conversion.json` 记录格式、大小与 SHA256。屏幕实际反射色与电脑预览不同，当前没有面板色度实测校准。

本机 ComfyUI 可先生成 1280×768 的横向图像，比例正好对应屏幕，再使用此工具。原始生成图和工作流保留在电脑，SD 相册目录仅放转换后的 BMP。

`tools/validate_bmp.c` 可与 `components/core/bmp.c` 一起编译为桌面校验器，使用设备同一解码实现验证图片完整像素，横屏用法：`validate_bmp photo1.bmp photo2.bmp`。
