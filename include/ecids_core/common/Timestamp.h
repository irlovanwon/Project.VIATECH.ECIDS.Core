/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Timestamp utilities (YYYYMMDD-HHMMSS-sss)
 * Date: 2026-06-18
 * Modification:
 */

#ifndef ECIDS_CORE_COMMON_TIMESTAMP_H
#define ECIDS_CORE_COMMON_TIMESTAMP_H

#pragma once

#include <string>
#include <cstdint>

namespace ecids_core {

class Timestamp {
public:
    static std::string now_string();
    static int64_t now_unix_ms();
};

} // namespace ecids_core

#endif // ECIDS_CORE_COMMON_TIMESTAMP_H
