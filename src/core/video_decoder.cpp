#include "core/video_decoder.h"

#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/rational.h>

#include <chrono>
#include <utility>

namespace {

constexpr int kOpenTimeoutMs = 5000;
constexpr int kReadTimeoutMs = 5000;

int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::string avErrorString(int err) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(err, buf, sizeof(buf));
    return buf;
}

std::string interruptError(int ret, const InterruptState* state,
                           const char* timeout_message) {
    if (ret != AVERROR_EXIT) {
        return avErrorString(ret);
    }
    if (state && state->stop.load(std::memory_order_relaxed)) {
        return {};
    }
    return timeout_message;
}

}  // namespace

VideoDecoder::~VideoDecoder() {
    close();
}

int VideoDecoder::interruptCallback(void* opaque) {
    auto* state = static_cast<InterruptState*>(opaque);
    if (!state) {
        return 0;
    }

    if (state->stop.load(std::memory_order_relaxed)) {
        return 1;
    }

    const int64_t deadline = state->deadline_ms.load(std::memory_order_relaxed);
    if (deadline > 0 && nowMs() > deadline) {
        return 1;
    }

    return 0;
}

bool VideoDecoder::open(const std::string& url, std::string& error,
                        InterruptState* interrupt) {
    error.clear();
    close();  // 清理上一次可能残留的资源
    interrupt_ = interrupt;

    fmt_ctx_ = avformat_alloc_context();
    if (!fmt_ctx_) {
        error = "avformat_alloc_context failed";
        return false;
    }
    if (interrupt_) {
        fmt_ctx_->interrupt_callback.callback = &VideoDecoder::interruptCallback;
        fmt_ctx_->interrupt_callback.opaque = interrupt_;
    }

    AVDictionary* options = nullptr;
    const bool is_rtsp = url.rfind("rtsp://", 0) == 0 || url.rfind("rtsps://", 0) == 0;
    if (is_rtsp) {
        av_dict_set(&options, "rtsp_transport", "tcp", 0);
        av_dict_set(&options, "timeout", "5000000", 0);
    }

    if (interrupt_) {
        interrupt_->deadline_ms = nowMs() + kOpenTimeoutMs;
    }
    int ret = avformat_open_input(&fmt_ctx_, url.c_str(), nullptr, &options);
    if (interrupt_) {
        interrupt_->deadline_ms = 0;
    }
    av_dict_free(&options);
    if (ret < 0) {
        error = interruptError(ret, interrupt_, "connect timeout");
        return false;
    }

    if (interrupt_) {
        interrupt_->deadline_ms = nowMs() + kOpenTimeoutMs;
    }
    ret = avformat_find_stream_info(fmt_ctx_, nullptr);
    if (interrupt_) {
        interrupt_->deadline_ms = 0;
    }
    if (ret < 0) {
        error = interruptError(ret, interrupt_, "stream info timeout");
        close();
        return false;
    }

    video_index_ = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_index_ < 0) {
        error = "no video stream";
        close();
        return false;
    }

    AVStream* stream = fmt_ctx_->streams[video_index_];
    const AVCodec* decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!decoder) {
        error = "decoder not found";
        close();
        return false;
    }

    codec_ctx_ = avcodec_alloc_context3(decoder);
    if (!codec_ctx_) {
        error = "avcodec_alloc_context3 failed";
        close();
        return false;
    }

    ret = avcodec_parameters_to_context(codec_ctx_, stream->codecpar);
    if (ret < 0) {
        error = avErrorString(ret);
        close();
        return false;
    }

    ret = avcodec_open2(codec_ctx_, decoder, nullptr);
    if (ret < 0) {
        error = avErrorString(ret);
        close();
        return false;
    }

    width_ = codec_ctx_->width;
    height_ = codec_ctx_->height;

    AVRational rate = av_guess_frame_rate(fmt_ctx_, stream, nullptr);
    double fps = (rate.num > 0 && rate.den > 0) ? av_q2d(rate) : 25.0;
    frame_interval_ms_ = fps > 0.0 ? static_cast<int>(1000.0 / fps + 0.5) : 40;
    if (frame_interval_ms_ < 1) {
        frame_interval_ms_ = 1;
    }

    packet_ = av_packet_alloc();
    frame_ = av_frame_alloc();
    if (!packet_ || !frame_) {
        error = "av_packet_alloc / av_frame_alloc failed";
        close();
        return false;
    }

    eof_ = false;
    waiting_keyframe_ = true;
    return true;
}

bool VideoDecoder::readFrame(FramePtr& out, std::string& error) {
    error.clear();

    while (true) {
        if (interrupt_ &&
            interrupt_->stop.load(std::memory_order_relaxed)) {
            error.clear();
            return false;
        }

        int ret = avcodec_receive_frame(codec_ctx_, frame_);

        if (ret == 0) {
            const AVPixelFormat src_format =
                static_cast<AVPixelFormat>(frame_->format);
            if (!sws_ || frame_->width != sws_width_ ||
                frame_->height != sws_height_ || src_format != sws_format_) {
                if (sws_) {
                    sws_freeContext(sws_);
                    sws_ = nullptr;
                }
                sws_ = sws_getContext(frame_->width, frame_->height, src_format,
                                      frame_->width, frame_->height,
                                      AV_PIX_FMT_RGB24, SWS_BILINEAR,
                                      nullptr, nullptr, nullptr);
                if (!sws_) {
                    error = "sws_getContext failed";
                    av_frame_unref(frame_);
                    return false;
                }
                sws_width_ = frame_->width;
                sws_height_ = frame_->height;
                sws_format_ = src_format;
            }

            auto result = std::make_shared<Frame>();
            result->width = frame_->width;
            result->height = frame_->height;
            result->pts = frame_->pts;
            result->decoded_ms = nowMs();
            result->rgb.resize(static_cast<size_t>(frame_->width) *
                               static_cast<size_t>(frame_->height) * 3);

            uint8_t* dst[1] = {result->rgb.data()};
            int dst_stride[1] = {frame_->width * 3};
            sws_scale(sws_, frame_->data, frame_->linesize, 0, frame_->height,
                      dst, dst_stride);

            out = std::move(result);
            av_frame_unref(frame_);
            return true;
        }

        if (ret != AVERROR(EAGAIN)) {
            if (ret == AVERROR_EXIT) {
                error = interruptError(ret, interrupt_, "read timeout");
                return false;
            }
            if (ret != AVERROR_EOF) {
                error = avErrorString(ret);
            }
            return false;
        }

        if (packet_pending_) {
            int sret = avcodec_send_packet(codec_ctx_, packet_);
            if (sret == 0) {
                av_packet_unref(packet_);
                packet_pending_ = false;
                continue;
            }
            if (sret == AVERROR(EAGAIN)) {
                continue;   // 输出还没取完，先 receive
            }
            error = avErrorString(sret);
            av_packet_unref(packet_);
            packet_pending_ = false;
            return false;
        }

        if (flush_pending_) {
            int sret = avcodec_send_packet(codec_ctx_, nullptr);
            if (sret == 0) {
                flush_pending_ = false;
                continue;
            }
            if (sret == AVERROR(EAGAIN)) {
                continue;
            }
            error = avErrorString(sret);
            flush_pending_ = false;
            return false;
        }

        if (interrupt_) {
            interrupt_->deadline_ms = nowMs() + kReadTimeoutMs;
        }
        ret = av_read_frame(fmt_ctx_, packet_);
        if (interrupt_) {
            interrupt_->deadline_ms = 0;
        }
        if (ret < 0) {
            if (ret == AVERROR_EXIT) {
                error = interruptError(ret, interrupt_, "read timeout");
                return false;
            }
            if (!eof_) {
                eof_ = true;
                flush_pending_ = true;
                continue;
            }
            return false;
        }

        if (packet_->stream_index != video_index_) {
            av_packet_unref(packet_);
            continue;
        }

        if (waiting_keyframe_) {
            if (!(packet_->flags & AV_PKT_FLAG_KEY)) {
                av_packet_unref(packet_);
                continue;
            }
            waiting_keyframe_ = false;
        }

        packet_pending_ = true;
    }
}

void VideoDecoder::close() {
    av_frame_free(&frame_);
    av_packet_free(&packet_);

    if (sws_) {
        sws_freeContext(sws_);
        sws_ = nullptr;
    }

    avcodec_free_context(&codec_ctx_);
    avformat_close_input(&fmt_ctx_);

    if (interrupt_) {
        interrupt_->deadline_ms = 0;
    }
    interrupt_ = nullptr;

    video_index_ = -1;
    width_ = 0;
    height_ = 0;
    frame_interval_ms_ = 40;
    sws_width_ = 0;
    sws_height_ = 0;
    sws_format_ = AV_PIX_FMT_NONE;
    eof_ = false;
    waiting_keyframe_ = false;
    packet_pending_ = false;
    flush_pending_ = false;
}
