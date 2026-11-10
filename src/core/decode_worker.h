#pragma once

#include "core/channel_stats.h"
#include "core/frame_queue.h"
#include "core/interrupt_state.h"
#include "core/video_decoder.h"

#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>
#include <thread>

class DecodeWorker {
public:
    DecodeWorker(std::string url, size_t queue_capacity, bool pace = false);
    ~DecodeWorker();

    DecodeWorker(const DecodeWorker&) = delete;
    DecodeWorker& operator=(const DecodeWorker&) = delete;

    void start();
    void stop();   // 只设置停止标志，阻塞读由中断回调打断
    void join();

    bool isRunning() const { return running_.load(); }
    bool isConnected() const { return connected_.load(); }
    std::string error() const;
    ChannelStats stats() const;

    FrameQueue& queue() { return queue_; }

private:
    void run();
    bool waitForRetry(int attempt);  // 等待期间收到 stop 时返回 false
    void setError(const std::string& error);

    std::string url_;
    FrameQueue queue_;
    VideoDecoder decoder_;  // 只在工作线程里用
    std::thread thread_;
    bool pace_ = false;
    bool is_rtsp_ = false;
    InterruptState interrupt_;

    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};
    std::atomic<uint64_t> decoded_frames_{0};
    std::atomic<int> reconnect_count_{0};
    std::atomic<int64_t> last_recovery_ms_{0};  // 上次恢复耗时（毫秒），0 表示未恢复

    mutable std::mutex error_mutex_;
    std::string error_;
};
