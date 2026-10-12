# 早期 ComfyUI 60 张相册制作记录

[文档首页](README.md) · [项目首页](../README.md)

> 历史制作示例。该批相册已在后续100张全球风景替换流程中备份并移除。当前网络制作与上传见[全球相册流程](world-album-workflow.md)。

2026-10-09 使用本机 ComfyUI 完成 60 张图片并复制到设备 SD 卡。工作流和相册清单保存于 `examples/comfyui/`；原图、BMP、预览、生成包和原始任务日志不提交到代码库。

| 相册 | 张数 |
|---|---:|
| 风景 | 4 |
| 科幻场景 | 3 |
| 二次元场景 | 3 |
| 日系二次元成年人物 | 20 |
| 中式旗袍：写实 / 二次元 | 10 / 10 |
| 科幻人物 | 10 |

原图 1280×768。风景、写实旗袍与科幻使用本机 DreamShaperXL Turbo，二次元使用本机 niji SDXL。`album-workflows.json` 保存每张图的提示词、随机种子、模型文件名、采样参数和完整 ComfyUI API 节点图。需要本机已有对应模型；同一种子在不同软件/硬件环境下不保证像素完全一致。

## 重现一张图片

在项目根目录运行，要求 ComfyUI 已在本机 8188 端口启动：

```powershell
$album = Get-Content ./examples/comfyui/album-workflows.json -Raw -Encoding utf8 | ConvertFrom-Json
$item = $album.items[0]
$body = @{ prompt = $item.workflow; client_id = [guid]::NewGuid().ToString() } | ConvertTo-Json -Depth 30
$job = Invoke-RestMethod http://127.0.0.1:8188/prompt -Method Post -ContentType 'application/json; charset=utf-8' -Body ([Text.Encoding]::UTF8.GetBytes($body))
$job.prompt_id
```

完成状态可通过 `/history/<prompt_id>` 查看，节点 `7` 给出输出图片文件名。原图保存在 ComfyUI 的 output 目录，按工作流中 `filename_prefix` 分组；工作流是 API 格式，不是前端画布导入格式。

将原图放入一个目录，使用 `tools/prepare_photos.py` 转成六色 BMP，步骤见 [图像格式](image-format.md)。60 张实制图片均通过固件同一 C 解码器检查，复制到 SD 后逐张读回 SHA256 一致。这里记录的是格式和复制验收，未逐张做上屏目视验收。

## SD 播放清单

`examples/comfyui/albums.json` 对应 6 个相册目录，默认逐相册、自然顺序播放，每张停留 900 秒，保留断点恢复。SD 根配置中使用 `album.root: "/albums"` 和 `album.manifest: "albums.json"`。将清单放在 SD `/albums/albums.json`，转换后的 BMP 放入清单指定的各目录。

更新 SD 前保存已有配置，避免覆盖同名图片；传输结束后安全弹出 SD，装回设备并重新开机。设备仍遵循保留最后成功照片的恢复规则，BOOT 短按推进到下一张；追加图片不会自动取消断点。

## 省电行为

当时 M1：换图时间到后定时唤醒，约 30 秒全刷新，完成后保留 10 秒常醒按键窗口，再进入深睡；下一间隔从刷新完成后计算。BOOT/KEY 也可唤醒。屏幕保留照片，功放关闭，屏幕内部高压关闭；所有辅助电源轨尚未完全关闭，休眠电流和电池续航尚未实测。
