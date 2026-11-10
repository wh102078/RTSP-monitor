#pragma once

#include "core/channel_config.h"

#include <string>
#include <vector>

bool loadChannelConfigs(const std::string& path,
                        std::vector<ChannelConfig>& out,
                        std::string& error);
