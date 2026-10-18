# SD 相册照片 API

[文档首页](README.md) · [项目首页](../README.md)

## 网页照片管理

在侧栏点击“照片库”，或访问 `/albums`。页面使用本文件描述的 API，所有资源内置固件，无需外部 CDN。相册以文件夹方式显示，提供新建、重命名和删除入口，点击打开后在同页浏览和上传照片，支持每页50张、筛选本页文件名、预览原始BMP、下载、逐张删除及多文件选择/拖放上传。选择相册仅改变管理目录，不改变轮播选择；停用相册会标注“未参与轮播”，播放选择和间隔在“播放设置”中调整。

上传显示单张进度，依次传输且每张等待索引完成后继续。页面接收JPEG、PNG和24位BMP原图，0.17.4起在本地逐张确认方向、完整显示/铺满裁剪和显示范围，整批确认后生成目标尺寸的24位BMP再上传。设备执行六色转换、尺寸和完整像素验证。遇到同名、非法BMP、忙碌或网络故障时停止本批，显示已完成数量，保留未完成选择；响应未确认时需刷新核对已写入文件，避免盲目重复上传。删除需确认，只移除所选文件；当前照片删除引起刷屏时等待约30秒。预览按需加载单张，列表不自动下载全部BMP。

设备离线时保留列表并禁用文件操作，连接恢复后可刷新；操作中串行访问设备，不同时请求缩略图。中文及特殊文件名用URL编码和纯文本DOM显示，预览关闭/换相册释放对象URL。原有状态页与播放设置保持可用。

## 文件接口

设备连接电脑或普通USB外部供电、保持唤醒并连上Wi-Fi时，通过局域网访问。这些接口仅访问根配置/相册清单已经定义的相册目录，包含停用的相册。照片非递归列出；子目录分组通过清单中的相册ID访问。不是任意SD文件浏览器，不提供根配置、Wi-Fi凭据或备份下载，0.14.35 起可通过相册管理接口新建与删除相册目录。

先读取 `GET /api/status`，取得 `control.token`。下表全部请求都带 `X-Album-Token: 令牌`；令牌在重启后变化，用于防止跨站请求，并非登录认证。只供现有局域网使用，服务不开放跨域读取。

| 方法与路径 | 功能 | 成功响应 |
| --- | --- | --- |
| `GET /api/albums` | 列出已配置相册ID、目录、启用状态及间隔 | 200，`root` 与 `albums` 数组 |
| `GET /api/files?album=ID&offset=0&limit=50` | 自然排序的BMP文件分页列表 | 200，`total/offset/limit/next_offset/files` |
| `GET /api/photo?album=ID&name=照片.bmp` | 下载原始照片，字节保持不变 | 200，`image/bmp`，分块响应 |
| `PUT /api/photo?album=ID&name=照片.bmp` | 上传原始BMP二进制数据流 | 201，`album/name/bytes/index_pending` |
| `DELETE /api/photo?album=ID&name=照片.bmp` | 删除指定BMP照片 | 200，`deleted/album/name/index_pending` |
| `POST /api/rescan` | 手动重新索引，异常后的修复入口 | 202，`job_id`，沿用控制命令状态 |

`album` 与 `name` 使用UTF-8 URL编码，只解码一次。拒绝重复/未知参数、NUL、路径分隔符、`..`、隐藏文件名、非BMP名称及FAT保留字符。不能以照片参数访问配置文件或目录。列表默认50项，每页1–100项，offset为0–2048；没有下一页时 `next_offset` 为null。每个相册最多列出2048个BMP；目录/非BMP/临时上传文件不出现在列表中。列表包含尺寸/头部不合格的现有BMP，并用 `header_valid` 标注，便于下载处理或删除；该字段不代表已检查每个像素的调色板。

`PUT /api/photo` 原样上传必须带 `Content-Length`，`Content-Type` 为 `image/bmp` 或 `application/octet-stream`，内容直接是BMP字节，不能使用 multipart/form-data。单文件上限2MiB；默认横向800×480，24位无压缩、正高度BMP，六色像素规则见 [图片格式](image-format.md)。旋转90/270度时要求480×800。原样上传接口不执行转换。同名（含FAT大小写等价名称）返回409，保留原文件；需要替换时由客户端显式删除旧照片再上传新文件。

使用8KiB缓冲流式写入同目录 `.epaper-upload.tmp`，收齐、flush/fsync并关闭后，重新读入并检查BMP头部及所有六色像素，验证成功才改名成正式照片。上传中断、超时、非法图或写入失败会丢弃临时文件。意外断电留下的隐藏临时文件不会播放，下次上传可以重用。整个网络传输预算约60秒，单次socket收发超时5秒；尚未提供分段续传或Range下载。

错误响应为JSON `error/message`：400参数错误、403缺少/错误令牌、404相册/照片不存在、409忙碌/同名/数量上限、413文件太大、415媒体类型错误、422BMP不合格、507SD空间不足/写入失败；其他SD读取问题返回500。刷屏、配置操作或重新索引期间，文件API返回409 `busy`，客户端应等待后重试。HTTP任务一次处理一个文件传输，传输期间其他请求可能等待，勿同时批量并发上传。

上传/删除成功代表文件已写入/移除，索引随后由应用任务重建。轮询状态里的 `control.files_revision/index_pending/index_error`：修订号增加且pending为false、error为空才表示索引完成。文件更新不修改相册清单的选择和间隔。当前照片仍存在时保持画面与原换图截止时间；当前照片被删除则选择剩余可播放照片；删除最后一张后保留屏幕，停止自动换图，新上传可播放照片后恢复。新文件上传到停用相册时仍需通过相册设置启用，才参与轮播。若索引失败，文件操作可能已经成功，检查 `index_error` 并提交 `POST /api/rescan` 的 `{}` 重试，不能直接假设文件未写入。

SD传输、配置写入、图片解码和显示通过共享锁互斥，禁止访问已卸载SD。电脑和普通USB外部供电保持在线，电池供电按计划深睡；睡眠时API不可访问；已开始的传输会先退出再停止服务和卸载SD。没有上板验证上传期间真实断电、SD满盘/掉卡等故障下FAT目录更新的可靠性。

## 网页构图与原图API的区别（0.17.4）

`GET /api/status`的`album.image_width/image_height`提供当前导入目标尺寸，横屏为800×480，竖屏为480×800。网页使用浏览器解码原文件、应用EXIF方向及用户旋转、合成白底与裁剪结果，将最终尺寸24位BMP用`image/bmp`发送到`PUT /api/upload`；文件名主体不变。任务的source_name/source_width/source_height反映实际传输的BMP，不是电脑原文件的元数据。

原文件8 MiB限制在网页选择时检查，发送的BMP约1.10 MiB。接口仍独立执行8 MiB上限，不接收构图参数；直接使用API上传JPEG/PNG/BMP保持原来的自动居中裁剪。API调用方需要人工选择构图时，可先合成目标尺寸的24位BMP再调用相同接口。网页操作取消不会创建设备转换任务。

## 原图自动转换（0.17.0）

`PUT /api/upload?album=ID&name=原图.jpg`：带控制令牌和 `Content-Length`，直接发送 JPEG、PNG 或24位 BI_RGB BMP 字节，不使用 multipart。0.17.1 起接受54字节至8 MiB（8388608字节，含上限），超出返回413 `upload_limit`。边长不超过12000像素、总像素不超过4000万。JPEG支持基线彩色/灰度及 EXIF 方向，拒绝渐进式、CMYK、损坏文件。PNG从0.17.2起支持彩色、灰度、索引色、透明及Adam7交错，透明合成白底，16位通道降到8位；拒绝APNG、CRC错误及截断文件。BMP支持正/负高度。尺寸、裁剪及六色约定见[照片格式](image-format.md)。

原图收齐后返回202及 `job_id/name/state`，后台转换、校验、发布并索引。输出同名主体的 `.bmp`，不能覆盖现有文件。PNG、JPEG和BMP同名主体会生成同一个输出名称。网络接收预算100秒，socket 超时20秒；没有断点续传。上传接收时 HTTP 服务一次处理一个请求，其他请求可能等待。客户端遇到不确定的网络超时，先查询任务和文件列表，不能直接重复上传。

`GET /api/upload`：带同一令牌，读取本次启动最近任务的实时状态；不依赖5秒状态缓存。`can_upload` 表示当前可以开始接收；客户端应先等待此字段为true，再发送大文件。此值是瞬时预检查，正式上传仍可能返回409。一次仅允许一个转换任务。文件夹新建、重命名、删除及原样上传在转换期间返回409。

| 字段 | 内容 |
|---|---|
| `job_id/album/source_name/name` | 任务编号、相册、来源名、最终 BMP 名 |
| `active/published/state/error` | 是否运行、是否已发布、阶段及错误 |
| `progress_percent` | 转换任务阶段进度0–100；收图阶段由页面显示传输字节进度 |
| `elapsed_ms/conversion_ms/paused_ms` | 包含接收的总耗时、接收后的耗时、已结束的暂停累计耗时 |
| `decode_resize_ms/dither_ms/write_ms/validate_ms` | 各阶段耗时，包含该阶段等待 |
| `source_width/source_height/width/height` | 来源与输出尺寸 |
| `workspace_bytes` | 主要转换缓冲区的分配上界，不是整机堆使用量 |

阶段为 `receiving/queued/decoding/resizing/dithering/writing/validating/publishing/indexing/waiting/succeeded/failed`。`waiting` 表示等待报时、显示或其他设备任务，完成后恢复原阶段。`succeeded` 表示正式 BMP 和索引均完成；失败但 `published=true` 时文件已经保存，应检查索引并重新扫描。根状态接口 `image_import` 提供相同任务字段的缓存快照。

临时文件为目标目录内 `.epaper-import-source.tmp` 和 `.epaper-import-output.tmp`。完整校验后原子改名，失败不会发布部分 BMP。任务结束清理临时数据，开机清理配置相册中这两个保留文件。转换期间保持唤醒；完成后恢复原电池休眠计划。转换低优先级运行并在 SD 操作之间让出资源，自动轮播和报时继续按原调度执行。

## Windows 命令行工具（原样 BMP）

`tools/sd_photo_api.py` 仅使用Python标准库，自动读取令牌，上传/删除后等待索引完成。下载要求新的本地目标文件，避免覆盖原图片。示例：

```powershell
python tools/sd_photo_api.py --url http://192.168.1.100 albums
python tools/sd_photo_api.py --url http://192.168.1.100 list comfy-qipao --limit 20
python tools/sd_photo_api.py --url http://192.168.1.100 upload comfy-qipao processed.bmp --name 061_new.bmp
python tools/sd_photo_api.py --url http://192.168.1.100 download comfy-qipao 061_new.bmp downloaded.bmp
python tools/sd_photo_api.py --url http://192.168.1.100 delete comfy-qipao 061_new.bmp
```

原始请求示例（令牌替换为状态接口返回值）：

```powershell
curl.exe -H "X-Album-Token: TOKEN" "http://192.168.1.100/api/files?album=comfy-qipao&limit=20"
curl.exe -X PUT -H "X-Album-Token: TOKEN" -H "Content-Type: image/bmp" --data-binary "@processed.bmp" "http://192.168.1.100/api/photo?album=comfy-qipao&name=061_new.bmp"
curl.exe -H "X-Album-Token: TOKEN" "http://192.168.1.100/api/photo?album=comfy-qipao&name=061_new.bmp" -o downloaded.bmp
curl.exe -X DELETE -H "X-Album-Token: TOKEN" "http://192.168.1.100/api/photo?album=comfy-qipao&name=061_new.bmp"
curl.exe -X POST -H "X-Album-Token: TOKEN" -H "Content-Type: application/json" --data "{}" "http://192.168.1.100/api/rescan"
```

页面导航、卡片和弹窗的当前规则见[Web控制台](web-layout.md)。照片库合并浏览与上传；侧栏只保留照片库和播放设置。

## Wi-Fi 可见网络扫描（0.14.21）

`POST /api/wifi-scan` 使用X-Album-Token和application/json空对象`{}`，返回202及job_id。扫描由应用任务执行，与换图、文件传输、保存配置及校时任务互斥，不修改Wi-Fi配置或设备当前照片。

`GET /api/wifi-scan` 使用同一令牌读取结果。排队/执行中返回job_id和state；完成后返回state=succeeded、networks、total、truncated。每个网络含ssid、rssi_dbm、channel、authmode（ESP-IDF认证枚举）和open；失败含error。最多返回信号最强的64个AP，隐藏SSID不列出。结果只保存在内存，页面以job_id匹配本轮扫描。扫描范围为ESP32-S3支持的2.4 GHz信道。


## 离线配网（0.14.23）

`GET /setup` 返回独立配网页面。`POST /api/wifi-provision` 使用控制令牌和空对象 `{}` 异步启动配网。`GET /api/captive-portal` 返回 `application/captive+json` 的 captive 和 user-portal-url 字段。`network.provisioning` 表示配网模式，`ap_ssid` / `ap_ip` 为开放热点信息。扫描和 Wi-Fi 保存 API 在 AP+STA 模式可用，其他控制命令及照片文件操作返回409。首次配网可启用此前关闭的 Wi-Fi / NTP。连接失败恢复原 SD 根配置，成功后热点保留30秒再关闭。详见 [离线配网](wifi-provisioning.md)。

## 相册文件夹维护（0.16.3）

`POST /api/album-create`：请求 `{ "schema_version": 1, "id": "world-asia", "enabled": false, "interval_seconds": 900 }`，在相册根目录新建与 ID 同名的目录，并保存到相册清单。ID 为1–63位英文字母、数字、下划线或连字符，不允许 Windows 保留目录名称。同名目录不复用。默认停用，间隔120–604800秒。0.16.3 起可添加可选 `directory` 字段指定显示和存储的文件夹名称；省略时沿用 ID。文件夹名称支持 UTF-8 中文，1–120字节，不允许路径分隔符、控制/保留字符、连续句点、首尾空格/末尾句点或 Windows 保留名称。

`POST /api/album-rename`：请求 `{ "schema_version": 1, "id": "world-asia", "directory": "亚洲风景" }`。仅修改真实目录和清单中的 directory，ID 保持不变，照片不重新编码，保留启用状态、停留时间及当前播放照片的截止时间；当前照片路径同步更新，无需重新刷屏。目标目录不能已存在，不能只更改大小写；共享或嵌套的配置目录拒绝操作。

重命名前写入 `<manifest>.rename-pending` 恢复记录。目录改名、扫描和清单保存失败时回滚；开机按已保存清单恢复中断的改名。恢复发生冲突时保留目录和记录并停止启动，供人工修复。此记录及 `.tmp` 文件名属于内部保留用途。

`POST /api/album-delete`：请求 `{ "schema_version": 1, "id": "world-asia" }`，删除空目录及对应清单记录。先用照片 API 删除照片。非空目录（包括隐藏或非BMP文件）会拒绝删除，防止误删未列出的内容；不支持递归删除。网页删除非空相册时，先确认照片数量及勾选永久删除，再通过照片 API 逐张删除和更新索引，最后调用相册删除；中断或遇到其他文件时会保留相册，并报告已删除数量。共享目录和嵌套目录拒绝删除。删除最后一个相册后允许清单为空，停止轮播并保留屏幕当前像素。

两者带 `X-Album-Token`、`Content-Type: application/json`，返回202和job_id，轮询 `/api/status` 的 control 记录确认成功。配置写入采用既有临时文件/备份恢复流程，目录操作失败不修改清单，清单写入失败尝试回滚目录。配网、刷屏及其他SD操作期间保持互斥。

命令行：`sd_photo_api.py --url http://设备IP create-album world-asia`；清空后使用 `delete-album world-asia`。`replace_album_library.py` 提供本机备份、完整替换和逐张下载SHA-256回读验证；`generate_world_album.py` 通过本机 ComfyUI 使用 Qwen Image 2.1 FP8生成100个全球地点，并转换为800×480六色24位BMP。

照片传输启用 TCP_NODELAY，减少分块头和像素数据之间的等待。客户端对明确409 busy等待重试，对明确408 upload_incomplete、服务器确认丢弃部分数据的上传最多重试3次；同名冲突和不确定的写入超时不会盲目重试。

运行期间 Wi-Fi 使用 WIFI_PS_NONE；完成电池模式的任务后仍按计划关闭 Wi-Fi 并深睡。该设置避免持续传输因 modem sleep 等待而超时。

完整替换示例（在项目目录执行，输出目录放在代码库外）：

```powershell
python tools/generate_world_album.py --output ../world-landscapes-100
python tools/replace_album_library.py --url http://192.168.1.100 --backup ../old-library --backup-only
python tools/replace_album_library.py --url http://192.168.1.100 --source ../world-landscapes-100 --backup ../old-library
```

生成可继续已有工作流和本地图片。替换会先核对完整备份与100张新BMP，删除旧照片和空相册，新建5个停用相册，逐张上传并下载比较SHA-256，全部成功后启用轮播。相册按亚洲、欧洲、非洲、美洲、大洋洲与极地区域分组，每组20张，轮播间隔15分钟。`installation.json`记录最终文件清单和回读验证数量。

上传响应包含SD刷盘和完整六色检查，客户端等待上限为120秒。替换中途失败时先核对设备文件清单；若5个目标相册均已创建且停用，可追加`--resume`继续。续传会拒绝多余相册、不同目录映射或未知照片，对已有文件先下载校验再跳过上传。超时不会自动重复发送写入请求。批量工具下载时遇到连接中断或超时，最多尝试4次；每次丢弃不完整响应，只有完整文件通过长度和SHA-256检查才计为成功。

照片上传、下载和完整六色验证使用32 KiB文件缓冲；照片传输块大小为8 KiB。

## 指定照片显示（0.17.3）

`POST /api/show-photo`，带 `X-Album-Token` 和 `Content-Type: application/json`：

```json
{"schema_version":1,"album":"family","name":"001.bmp"}
```

仅允许这三个字段，拒绝重复字段、未知相册、非法文件名及不合格BMP。返回202和job_id，轮询 `/api/status` 的 `control`；kind为 `show-photo`，只有state=succeeded才表示显示及状态保存完成。缺失或损坏照片使任务failed，保留原播放位置；状态保存失败可能已显示，错误文本会注明。上传、索引或其他控制命令忙时返回409。报时期间可排队，显示仍等待资源窗口。

对已启用相册，成功后从指定照片在顺序/随机队列中的后继继续，其他相册队列不变。对停用相册，只点播一张，启用选择和SD清单不修改；停留该相册间隔后恢复原队列。没有启用照片时停止自动换图并保留点播图。成功后的间隔从刷新结束起算，自动切换遵循夜间暂停。当前同名同内容照片的重复请求不刷屏、不重置倒计时。

点播位置和停用相册标记保存在NVS；正常深睡唤醒保留截止时间，普通重启按原恢复规则开始完整间隔。文件重扫及文件夹改名保留仍有效的点播照片；删除当前照片后选择原轮播中的照片。显式保存播放相册选择会结束停用相册的点播，应用所选队列。

照片库卡片提供“显示此照片”，在固定大小的按钮区域显示进度和当前标记；预览和预览左右箭头仍只影响浏览器。

## 相框摆放方向（0.17.5）

状态album.image_rotation_degrees提供固件现有的0/90/180/270度解码旋转。网页根据用户指定的物理相框方向构图，再反向旋转并抵消此配置后上传BMP；因此该字段与image_width/image_height共同用于成图，既有直接上传API不变。

## 上传重名提示（0.17.6）

网页在构图确认前分页检查目标相册，按最终BMP名忽略大小写比较；发现冲突整批不传输。正式上传仍由设备检查，409 exists表示输出已存在，500 sd_access_failed表示输出路径无法访问，不再共用exists。文件仍不覆盖。晚到的同名冲突显示中文文件名与处理方式；0张完成时不显示批次完成数字，已完成部分保留计数。
