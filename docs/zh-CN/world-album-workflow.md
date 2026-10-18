# 全球风景相册网络替换

[English](../world-album-workflow.md) | 简体中文


[文档首页](README.md) · [项目首页](../../README.zh-CN.md)

## 生成与格式

本机 ComfyUI 使用 Qwen Image 2.1 UC FP8、Qwen3-VL 8B BF16 文本编码器和 Qwen Image 2.1 BF16 VAE。每个地点使用独立提示词和种子，生成1280×768横向图片。采样为25步、Euler/simple、CFG 1.0。

图片缩放到800×480，经过轻度对比度与锐化处理，再按黑、白、黄、红、蓝、绿六色进行 Floyd–Steinberg 抖动。输出24位无压缩BMP，每张1,152,054字节。转换后不再缩放。固件BMP解码器检查文件头、尺寸和所有像素颜色。

五个相册各20张，启用后统一每15分钟换图：

|相册ID|范围|
|---|---|
|world-asia|亚洲|
|world-europe|欧洲|
|world-africa|非洲|
|world-americas|美洲|
|world-oceania-polar|大洋洲与极地|

这些图片是AI生成的风景作品。

## 网络操作

`tools/generate_world_album.py` 生成图片、保存原图/工作流并转换BMP。`tools/replace_album_library.py` 先备份旧相册与照片，再检查100张本地BMP，删除旧照片及空相册，创建停用的新相册，逐张上传并下载比较SHA-256。全部校验成功后启用新相册。命令见[SD照片API文档](http-files.md)。

固件新增相册创建和空相册删除API。删除非空目录会失败；隐藏或非BMP内容不会递归删除。全局Wi-Fi和校时设置保留。

## 批量传输改进

- HTTP路由容量增加到32，容纳新增接口。
- 照片连接启用TCP_NODELAY，运行时Wi-Fi采用WIFI_PS_NONE。
- 上传、下载和六色验证使用32 KiB文件缓冲，网络传输块为8 KiB。
- 单次socket收发等待20秒，文件传输保留60秒总截止时间。
- 上传响应等待120秒，涵盖SD刷盘和完整像素检查。
- 下载中断最多尝试4次，丢弃不完整响应，完整文件再比较SHA-256。
- 只有服务器明确返回408 upload_incomplete并确认部分数据已丢弃时，才有限重试上传。明确409 busy会等待；同名冲突和不确定的客户端写入超时不会盲目重发。
- 显式续传检查相册、目录及已有文件，已有照片下载校验后跳过写入。

## 本机保留

原图、BMP、转换预览、工作流、旧相册备份、上传记录及`installation.json`保存在代码库外。仓库只提交代码、工具和文档，不包含私人照片、旧固件、Wi-Fi凭据或令牌。

实际验收结果见[验证记录](validation.md)。
