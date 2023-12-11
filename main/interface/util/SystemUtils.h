//
// Created by v2ray on 2023/3/3.
//

#ifndef GPT3BOT_SYSTEMUTILS_H
#define GPT3BOT_SYSTEMUTILS_H

#include "boost/algorithm/string.hpp"
#include "boost/lexical_cast.hpp"
#include "iostream"
#include "string"
#include "vector"
#include "chrono"

namespace util {

    long long currentTimeMillis();
    std::string currentTimeFormatted();
    std::string ms_to_formatted_time(long long time_ms);
    short timezone_offset_minutes(long long time_ms);
    std::string system_proxy();
    void free_proxy_factory();
} // util

#endif //GPT3BOT_SYSTEMUTILS_H
