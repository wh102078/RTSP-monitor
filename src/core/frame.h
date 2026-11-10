#pragma once

#include <cstdint>
#include <memory>
#include <vector>

struct Frame {
    std::vector<uint8_t> rgb;  // width * height * 3
    int width = 0;
    int height = 0;
    int64_t pts = 0;
    int64_t decoded_ms = 0;    // 解码完成时间，用来算显示延迟
};

using FramePtr = std::shared_ptr<Frame>;
