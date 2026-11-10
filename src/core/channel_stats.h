#pragma once

#include <cstdint>
#include <string>

struct ChannelStats {
    uint64_t decoded_frames = 0;
    uint64_t dropped_frames = 0;
    int reconnect_count = 0;
    int64_t last_recovery_ms = 0;  // 上次恢复耗时（毫秒），0 表示未恢复
    bool connected = false;
    std::string last_error;
};
