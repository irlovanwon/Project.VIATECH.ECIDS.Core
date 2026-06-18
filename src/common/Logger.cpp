/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Logger implementation
 * Date: 2026-06-18
 * Modification:
 */

#include "ecids_core/common/Logger.h"
#include "ecids_core/common/Timestamp.h"

#include <iostream>
#include <mutex>

namespace ecids_core {

static std::mutex g_log_mutex;

static const char* level_str(Logger::Level level) {
    switch (level) {
        case Logger::DEBUG: return "DEBUG";
        case Logger::INFO:  return "INFO";
        case Logger::WARN:  return "WARN";
        case Logger::ERROR: return "ERROR";
        case Logger::FATAL: return "FATAL";
        default:            return "UNKNOWN";
    }
}

void Logger::log(Level level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    std::cerr << Timestamp::now_string() << " | " << level_str(level) << " | Core | " << msg << std::endl;
}

void Logger::info(const std::string& msg)  { log(INFO, msg); }
void Logger::warn(const std::string& msg)  { log(WARN, msg); }
void Logger::error(const std::string& msg) { log(ERROR, msg); }
void Logger::debug(const std::string& msg) { log(DEBUG, msg); }

} // namespace ecids_core
