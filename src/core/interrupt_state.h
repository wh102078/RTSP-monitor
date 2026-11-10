#pragma once

#include <atomic>
#include <cstdint>

struct InterruptState {
    std::atomic<bool> stop{false};
    std::atomic<int64_t> deadline_ms{0};  // steady_clock 毫秒；0 表示不设超时
};
