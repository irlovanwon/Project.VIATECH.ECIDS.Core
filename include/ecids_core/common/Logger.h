/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Thread-safe singleton logger
 * Date: 2026-06-18
 * Modification:
 */

#ifndef ECIDS_CORE_COMMON_LOGGER_H
#define ECIDS_CORE_COMMON_LOGGER_H

#pragma once

#include <string>

namespace ecids_core {

class Logger {
public:
    enum Level { DEBUG, INFO, WARN, ERROR, FATAL };

    static void info(const std::string& msg);
    static void warn(const std::string& msg);
    static void error(const std::string& msg);
    static void debug(const std::string& msg);

private:
    static void log(Level level, const std::string& msg);
};

} // namespace ecids_core

#endif // ECIDS_CORE_COMMON_LOGGER_H
