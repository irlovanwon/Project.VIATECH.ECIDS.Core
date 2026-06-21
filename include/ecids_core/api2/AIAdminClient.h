/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: API2b HTTPS client - admin to AIVisionServerDealer
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented with libcurl
 */

#ifndef ECIDS_CORE_API2_AIADMINCLIENT_H
#define ECIDS_CORE_API2_AIADMINCLIENT_H

#pragma once

#include <string>
#include <mutex>

namespace ecids_core {

class AIAdminClient {
public:
    void init(const std::string& host, int port, int timeout_ms = 5000);

    std::string send_command(const std::string& action,
                             const std::string& params_json = "");

    std::string check_status();
    std::string set_parameter(const std::string& name, const std::string& value);
    std::string get_parameter(const std::string& name);

    const std::string& host() const { return host_; }
    int port() const { return port_; }

private:
    std::string host_;
    int port_ = 8445;
    int timeout_ms_ = 5000;
    std::mutex mutex_;
};

} // namespace ecids_core

#endif // ECIDS_CORE_API2_AIADMINCLIENT_H
