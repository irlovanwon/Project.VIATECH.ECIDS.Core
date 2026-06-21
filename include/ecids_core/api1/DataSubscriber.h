/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: API1a ZMQ SUB - data from StereoCamera
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented per-channel subscriber threads
 */

#ifndef ECIDS_CORE_API1_DATASUBSCRIBER_H
#define ECIDS_CORE_API1_DATASUBSCRIBER_H

#pragma once

#include "ecids_core/common/Types.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <unordered_map>

namespace ecids_core {

class DataSubscriber {
public:
    using DataCallback = std::function<void(const std::string& channel, const DataBundle& bundle)>;

    DataSubscriber();
    ~DataSubscriber();

    void add_channel(const std::string& name, const std::string& endpoint);
    void set_callback(DataCallback cb);

    void start();
    void stop();

    bool is_running() const { return running_; }

private:
    void subscriber_loop_(const std::string& channel, const std::string& endpoint);

    struct ChannelInfo {
        std::string name;
        std::string endpoint;
    };

    std::vector<ChannelInfo> channels_;
    DataCallback callback_;
    std::vector<std::thread> threads_;
    std::atomic<bool> running_{false};
    void* zmq_ctx_ = nullptr;
};

} // namespace ecids_core

#endif // ECIDS_CORE_API1_DATASUBSCRIBER_H
