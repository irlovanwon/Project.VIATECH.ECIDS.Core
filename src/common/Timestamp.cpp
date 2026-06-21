/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Timestamp utilities implementation
 * Date: 2026-06-18
 * Modification: 2026-06-21 Added date/time component accessors for database paths
 */

#include "ecids_core/common/Timestamp.h"

#include <chrono>
#include <cstdio>
#include <ctime>

namespace ecids_core {

static TimeParts to_parts(std::time_t t, int ms) {
    std::tm tm{};
    localtime_r(&t, &tm);
    return TimeParts{
        tm.tm_year + 1900,
        tm.tm_mon + 1,
        tm.tm_mday,
        tm.tm_hour,
        tm.tm_min,
        tm.tm_sec,
        ms
    };
}

std::string Timestamp::now_string() {
    auto parts = now_parts();
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d%02d%02d-%02d%02d%02d-%03d",
                  parts.year, parts.month, parts.day,
                  parts.hour, parts.min, parts.sec, parts.ms);
    return std::string(buf);
}

int64_t Timestamp::now_unix_ms() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               now.time_since_epoch()).count();
}

TimeParts Timestamp::now_parts() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) % 1000;
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    return to_parts(t, static_cast<int>(ms.count()));
}

std::string Timestamp::now_db_folder_part() {
    return db_folder_part_from_parts(now_parts());
}

std::string Timestamp::now_db_date_path() {
    return db_date_path_from_parts(now_parts());
}

std::string Timestamp::db_date_path_from_parts(const TimeParts& t) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d/%04d-%02d/%04d-%02d-%02d",
                  t.year, t.year, t.month, t.year, t.month, t.day);
    return std::string(buf);
}

std::string Timestamp::db_folder_part_from_parts(const TimeParts& t) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d-%d-%02d-%02d",
                  t.year, t.month, t.day, t.hour, t.min, t.sec);
    return std::string(buf);
}

} // namespace ecids_core
