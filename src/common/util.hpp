#pragma once

#include "common/wlroots.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace biway {

inline void log_info(const std::string& msg) {
    wlr_log(WLR_INFO, "%s", msg.c_str());
}

inline void log_error(const std::string& msg) {
    wlr_log(WLR_ERROR, "%s", msg.c_str());
}

inline void log_debug(const std::string& msg) {
    wlr_log(WLR_DEBUG, "%s", msg.c_str());
}

} // namespace biway
