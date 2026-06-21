/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: API2a ZMQ DEALER/DEALER - AI detection
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented DEALER with File/Http/Binary modes
 */

#ifndef ECIDS_CORE_API2_DETECTIONDEALER_H
#define ECIDS_CORE_API2_DETECTIONDEALER_H

#pragma once

#include "ecids_core/common/Types.h"
#include <string>
#include <thread>
#include <atomic>
#include <functional>

namespace ecids_core {

class DetectionDealer {
public:
    using ResultCallback = std::function<void(const DetectionResponse&)>;

    DetectionDealer();
    ~DetectionDealer();

    void init(const std::string& endpoint, const std::string& identity,
              int sndhwm = 10, int rcvhwm = 10, int poll_timeout_ms = 100);

    void start();
    void stop();

    void send_file_request(const std::string& transaction_id,
                           const std::vector<std::string>& image_uris,
                           const std::vector<std::string>& filenames);

    void send_binary_request(const std::string& transaction_id,
                             const uint8_t* left_data, size_t left_size,
                             const uint8_t* right_data, size_t right_size);

    bool is_running() const { return running_; }

    void set_result_callback(ResultCallback cb) { callback_ = std::move(cb); }

private:
    void poll_loop_();

    std::string endpoint_;
    std::string identity_;
    int sndhwm_ = 10;
    int rcvhwm_ = 10;
    int poll_timeout_ms_ = 100;

    void* zmq_ctx_ = nullptr;
    void* sock_ = nullptr;
    std::thread thread_;
    std::atomic<bool> running_{false};
    ResultCallback callback_;
};

} // namespace ecids_core

#endif // ECIDS_CORE_API2_DETECTIONDEALER_H
