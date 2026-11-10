# RTSP 多路监控客户端

基于 C++17、Qt6 和 FFmpeg 的 Linux 桌面客户端，用来同时预览多路 RTSP 摄像头。每路使用独立解码线程，解码后的 RGB 帧交给 Qt 界面显示，断线后自动重连。

![运行截图](screenshot.png)

## 功能

- 多路 RTSP 或本地视频同时预览，通道数由配置决定
- 每路独立解码线程，互不阻塞
- 有界帧队列，满时丢弃最旧帧，避免延迟持续累积
- 断线重连，退避间隔 1 / 2 / 4 / 8 / 10 秒，稳定 15 秒后重新计数
- 重连后等待关键帧再显示，减少花屏
- 通过 FFmpeg 的 `interrupt_callback` 打断阻塞 I/O，关闭窗口时不用等网络超时
- 每路显示分辨率、FPS、丢帧数、重连次数和解码到显示的延迟

## 构建

依赖：

- C++17 编译器
- Qt 6 Widgets
- FFmpeg 7.x（libavformat、libavcodec、libswscale、libavutil）
- make、pkg-config

Ubuntu 上安装依赖：

```bash
sudo apt install build-essential pkg-config qt6-base-dev \
                 libavformat-dev libavcodec-dev libswscale-dev libavutil-dev
```

编译：

```bash
make
```

生成的程序为 `./rtsp_monitor`。

## 运行

直接传本地文件或 RTSP 地址，参数个数不限：

```bash
./rtsp_monitor media/gate.mp4 media/warehouse.mp4 media/parking.mp4 media/backdoor.mp4
./rtsp_monitor rtsp://127.0.0.1:8554/gate
```

不带参数时读取 `config/channels.json`：

```bash
./rtsp_monitor
```

### 本地测试

用 [mediamtx](https://github.com/bluenviron/mediamtx) 模拟 4 路 RTSP：

```bash
mediamtx

ffmpeg -re -stream_loop -1 -i media/gate.mp4 -an -c copy -f rtsp rtsp://127.0.0.1:8554/gate
ffmpeg -re -stream_loop -1 -i media/warehouse.mp4 -an -c copy -f rtsp rtsp://127.0.0.1:8554/warehouse
ffmpeg -re -stream_loop -1 -i media/parking.mp4 -an -c copy -f rtsp rtsp://127.0.0.1:8554/parking
ffmpeg -re -stream_loop -1 -i media/backdoor.mp4 -an -c copy -f rtsp rtsp://127.0.0.1:8554/backdoor
```

## 配置

`config/channels.json` 格式如下：

```json
{
  "channels": [
    { "name": "通道 1 · 大门", "url": "rtsp://127.0.0.1:8554/gate" },
    { "name": "通道 2 · 仓库", "url": "rtsp://127.0.0.1:8554/warehouse" }
  ]
}
```

- `name` 是界面标题，留空时自动生成；
- `url` 可以是 RTSP 地址，也可以是本地文件；
- 相对路径按配置文件所在目录解析。

### 主要模块

| 模块 | 作用 |
|---|---|
| `VideoDecoder` | 打开输入、读包、解码、YUV 转 RGB |
| `FrameQueue` | 有界帧队列，支持阻塞和非阻塞取帧 |
| `DecodeWorker` | 每路一个线程，负责重连、关键帧等待和运行统计 |
| `MainWindow` / `ChannelTile` / `VideoWidget` | 多路网格、单路状态、视频绘制 |
| `config_loader` | 读取 JSON 通道配置 |

## 性能

测试环境：2 vCPU / 3.3 GB Ubuntu 虚拟机，854×480@25fps，软件解码，offscreen 渲染。

| 场景 | CPU | 内存 | 解码到显示延迟 |
|---|---|---|---|
| 4 路，30 秒 | 27% | 113 MB | 11.9 ms |
| 9 路，15 秒 | 57% | 181 MB | 14.2 ms |

4 路连续运行 5 分钟，内存稳定在 110 MB；断开一路的源 6 秒后恢复，约 2 秒重连，其他通道不受影响。这里的延迟只计算解码完成到 UI 绘制，不包含摄像头到客户端的网络时间。

## 目录结构

```
src/main.cpp  程序入口
src/core/     解码器、帧队列、解码线程、配置结构体、运行统计
src/ui/       Qt 窗口、通道卡片、视频控件、配置读取
tests/        帧队列、解码器、解码线程测试
config/       通道配置
media/        测试视频
```

## 素材与许可

`media/` 中的视频截取自 Blender Foundation 的开源电影，授权信息见 [media/README.md](media/README.md)。

代码使用 MIT License。
