#pragma once

#include "core/frame.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>

class FrameQueue {
public:
    explicit FrameQueue(size_t capacity);

    void push(FramePtr frame);          // 队列满时丢弃最旧帧
    bool pop(FramePtr& out);            // 阻塞；stop 后取完剩余帧返回 false
    bool tryPopLatest(FramePtr& out);   // 非阻塞，返回最新帧并清空队列
    void stop();                        // 单向，唤醒所有等待者
    size_t dropped() const;

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<FramePtr> queue_;
    size_t capacity_;
    bool stopped_ = false;
    std::atomic<size_t> dropped_{0};
};
