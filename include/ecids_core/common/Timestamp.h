/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Timestamp utilities (YYYYMMDD-HHMMSS-sss)
 * Date: 2026-06-18
 * Modification: 2026-06-21 Added date/time component accessors for database paths
 */

#ifndef ECIDS_CORE_COMMON_TIMESTAMP_H
#define ECIDS_CORE_COMMON_TIMESTAMP_H

#pragma once

#include <string>
#include <cstdint>

namespace ecids_core {

struct TimeParts {
    int year;
    int month;
    int day;
    int hour;
    int min;
    int sec;
    int ms;
};

class Timestamp {
public:
    static std::string now_string();
    static int64_t now_unix_ms();

    static TimeParts now_parts();

    static std::string now_db_folder_part();
    static std::string now_db_date_path();

    static std::string db_date_path_from_parts(const TimeParts& t);
    static std::string db_folder_part_from_parts(const TimeParts& t);
};

} // namespace ecids_core

#endif // ECIDS_CORE_COMMON_TIMESTAMP_H
