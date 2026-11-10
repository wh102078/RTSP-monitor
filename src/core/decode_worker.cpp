#include "core/decode_worker.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <utility>

DecodeWorker::DecodeWorker(std::string url, size_t queue_capacity, bool pace)
    : url_(std::move(url)), queue_(queue_capacity), pace_(pace) {
    is_rtsp_ = url_.rfind("rtsp://", 0) == 0 || url_.rfind("rtsps://", 0) == 0;
}

DecodeWorker::~DecodeWorker() {
    stop();
    join();
}

void DecodeWorker::start() {
    if (thread_.joinable()) {
        return;
    }

    stop_requested_ = false;
    interrupt_.stop = false;
    interrupt_.deadline_ms = 0;
    connected_ = false;
    decoded_frames_ = 0;
    reconnect_count_ = 0;
    last_recovery_ms_ = 0;
    setError("");
    running_ = true;
    thread_ = std::thread(&DecodeWorker::run, this);
}

void DecodeWorker::stop() {
    stop_requested_ = true;
    interrupt_.stop = true;
}

void DecodeWorker::join() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

std::string DecodeWorker::error() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return error_;
}

void DecodeWorker::setError(const std::string& error) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    error_ = error;
}

ChannelStats DecodeWorker::stats() const {
    ChannelStats stats;
    stats.decoded_frames = decoded_frames_.load();
    stats.dropped_frames = queue_.dropped();
    stats.reconnect_count = reconnect_count_.load();
    stats.last_recovery_ms = last_recovery_ms_.load();
    stats.connected = connected_.load();

    std::lock_guard<std::mutex> lock(error_mutex_);
    stats.last_error = error_;
    return stats;
}

bool DecodeWorker::waitForRetry(int attempt) {
    int delay_ms = 1000;
    for (int i = 0; i < attempt && delay_ms < 10000; ++i) {
        delay_ms *= 2;
    }
    if (delay_ms > 10000) {
        delay_ms = 10000;
    }

    std::fprintf(stderr, "[DecodeWorker] reconnect in %d ms\n", delay_ms);

    constexpr int kStepMs = 100;
    for (int waited = 0; waited < delay_ms; waited += kStepMs) {
        if (stop_requested_) {
            return false;
        }
        const int remaining = delay_ms - waited;
        std::this_thread::sleep_for(
            std::chrono::milliseconds(std::min(kStepMs, remaining)));
    }
    return !stop_requested_;
}

void DecodeWorker::run() {
    int attempt = 0;
    bool recovering = false;
    std::chrono::steady_clock::time_point recovery_start;

    while (!stop_requested_) {
        std::string error;

        if (!decoder_.open(url_, error, &interrupt_)) {
            connected_ = false;
            if (stop_requested_) {
                break;  // 停止导致的 open 失败不算错误
            }
            setError(error);

            if (!is_rtsp_) {
                break;  // 本地文件不做重试
            }
            if (!recovering) {
                recovering = true;
                recovery_start = std::chrono::steady_clock::now();
            }
            ++reconnect_count_;
            if (!waitForRetry(attempt++)) {
                break;
            }
            continue;
        }

        connected_ = true;
        setError("");
        const auto connected_at = std::chrono::steady_clock::now();

        if (recovering) {
            last_recovery_ms_ =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    connected_at - recovery_start)
                    .count();
            recovering = false;
        }

        while (!stop_requested_) {
            FramePtr frame;
            if (!decoder_.readFrame(frame, error)) {
                break;
            }

            ++decoded_frames_;

            if (stop_requested_) {
                break;
            }

            queue_.push(std::move(frame));

            if (pace_) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(decoder_.frameIntervalMs()));
            }
        }

        decoder_.close();
        connected_ = false;

        if (stop_requested_) {
            break;
        }

        if (!error.empty()) {
            setError(error);
        }

        if (!is_rtsp_) {
            break;
        }

        const auto connected_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - connected_at)
                .count();
        if (connected_ms >= 15000) {
            attempt = 0;
        }

        if (!recovering) {
            recovering = true;
            recovery_start = std::chrono::steady_clock::now();
        }
        ++reconnect_count_;

        if (!waitForRetry(attempt++)) {
            break;
        }
    }

    decoder_.close();
    queue_.stop();     // 重连期间不关闭队列，只在退出时关闭
    connected_ = false;
    running_ = false;
}
