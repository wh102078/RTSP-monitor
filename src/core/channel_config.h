#pragma once

#include <string>

struct ChannelConfig {
    std::string name;
    std::string url;  // 本地文件路径或 rtsp:// 地址
};
