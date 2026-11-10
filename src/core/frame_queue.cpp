#include "core/frame_queue.h"

#include <utility>

FrameQueue::FrameQueue(size_t capacity)
    : capacity_(capacity == 0 ? 1 : capacity) {}

void FrameQueue::push(FramePtr frame) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (queue_.size() >= capacity_) {
        queue_.pop_front();  // 满了丢最旧帧
        ++dropped_;
    }

    queue_.push_back(std::move(frame));

    cv_.notify_one();
}

bool FrameQueue::pop(FramePtr& out) {
    std::unique_lock<std::mutex> lock(mutex_);

    cv_.wait(lock, [this] { return !queue_.empty() || stopped_; });

    if (queue_.empty()) {
        return false;
    }

    out = std::move(queue_.front());
    queue_.pop_front();
    return true;
}

bool FrameQueue::tryPopLatest(FramePtr& out) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (queue_.empty()) {
        return false;
    }

    out = std::move(queue_.back());  // 只保留最新帧
    queue_.clear();
    return true;
}

void FrameQueue::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    stopped_ = true;
    cv_.notify_all();
}

size_t FrameQueue::dropped() const {
    return dropped_.load();
}
