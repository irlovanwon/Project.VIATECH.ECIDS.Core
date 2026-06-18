/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Response codes and helpers
 * Date: 2026-06-18
 * Modification:
 */

#ifndef ECIDS_CORE_COMMON_RESPONSE_H
#define ECIDS_CORE_COMMON_RESPONSE_H

#pragma once

#include <string>

namespace ecids_core {

enum class ResponseCode {
    Success      = 0,
    Error        = 1,
    NotReady     = 2,
    AlreadyInit  = 3,
    InvalidParam = 4,
    Unavailable  = 5,
    ModeConflict = 6
};

inline const char* response_code_name(ResponseCode code) {
    switch (code) {
        case ResponseCode::Success:      return "Success";
        case ResponseCode::Error:        return "Error";
        case ResponseCode::NotReady:     return "NotReady";
        case ResponseCode::AlreadyInit:  return "AlreadyInit";
        case ResponseCode::InvalidParam: return "InvalidParam";
        case ResponseCode::Unavailable:  return "Unavailable";
        case ResponseCode::ModeConflict: return "ModeConflict";
        default:                         return "Unknown";
    }
}

} // namespace ecids_core

#endif // ECIDS_CORE_COMMON_RESPONSE_H
