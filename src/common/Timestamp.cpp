/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Timestamp utilities implementation
 * Date: 2026-06-18
 * Modification:
 */

#include "ecids_core/common/Timestamp.h"

#include <chrono>
#include <cstdio>
#include <ctime>

namespace ecids_core {

std::string Timestamp::now_string() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) % 1000;
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d%02d%02d-%02d%02d%02d-%03d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec,
                  static_cast<int>(ms.count()));
    return std::string(buf);
}

int64_t Timestamp::now_unix_ms() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               now.time_since_epoch()).count();
}

} // namespace ecids_core
