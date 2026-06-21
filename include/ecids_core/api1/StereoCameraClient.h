/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: API1b HTTPS client - commands to StereoCamera
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented with libcurl
 */

#ifndef ECIDS_CORE_API1_STEREOCAMERACLIENT_H
#define ECIDS_CORE_API1_STEREOCAMERACLIENT_H

#pragma once

#include <string>
#include <mutex>

namespace ecids_core {

class StereoCameraClient {
public:
    void init(const std::string& host, int port, int timeout_ms = 5000);

    std::string send_command(const std::string& method, const std::string& path,
                             const std::string& body = "");

    std::string check_status();
    std::string connect();
    std::string disconnect();
    std::string start_capture(const std::string& data_types_json);
    std::string stop_capture(const std::string& data_types_json);
    std::string set_parameter(const std::string& name, const std::string& value);
    std::string get_parameter(const std::string& name);
    std::string activate_channel(const std::string& channel);
    std::string deactivate_channel(const std::string& channel);

    const std::string& host() const { return host_; }
    int port() const { return port_; }

private:
    std::string host_;
    int port_ = 9443;
    int timeout_ms_ = 5000;
    std::mutex mutex_;
};

} // namespace ecids_core

#endif // ECIDS_CORE_API1_STEREOCAMERACLIENT_H
