#pragma once

#include "core/frame.h"
#include "core/interrupt_state.h"

#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

class VideoDecoder {
public:
    VideoDecoder() = default;
    ~VideoDecoder();

    VideoDecoder(const VideoDecoder&) = delete;
    VideoDecoder& operator=(const VideoDecoder&) = delete;

    bool open(const std::string& url, std::string& error,
              InterruptState* interrupt = nullptr);

    bool readFrame(FramePtr& out, std::string& error);

    void close();

    int width() const { return width_; }
    int height() const { return height_; }
    int frameIntervalMs() const { return frame_interval_ms_; }

private:
    static int interruptCallback(void* opaque);

    InterruptState* interrupt_ = nullptr;  // 不拥有，生命周期由 DecodeWorker 保证

    AVFormatContext* fmt_ctx_ = nullptr;
    AVCodecContext* codec_ctx_ = nullptr;
    SwsContext* sws_ = nullptr;
    AVPacket* packet_ = nullptr;
    AVFrame* frame_ = nullptr;

    int video_index_ = -1;
    int width_ = 0;
    int height_ = 0;
    int frame_interval_ms_ = 40;

    // 当前 sws 上下文对应的源参数；分辨率/格式变化时重建
    int sws_width_ = 0;
    int sws_height_ = 0;
    AVPixelFormat sws_format_ = AV_PIX_FMT_NONE;

    bool eof_ = false;
    bool waiting_keyframe_ = false;  // 连接后先等关键帧
    bool packet_pending_ = false;    // send 返回 EAGAIN，包还没被接收
    bool flush_pending_ = false;     // EOF 后的空包还没发送
};
