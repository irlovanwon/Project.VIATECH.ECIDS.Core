/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: API3b ZMQ PUB - data publishing with ZMQ_IMMEDIATE + zero-copy
 * Date: 2026-06-18
 * Modification: 2026-07-02 Added ZMQ_IMMEDIATE=1, true zero-copy (zmq_msg_init_data)
 */

#ifndef ECIDS_CORE_API3_DATAPUBLISHER_H
#define ECIDS_CORE_API3_DATAPUBLISHER_H

#pragma once

#include "ecids_core/common/Types.h"
#include <string>
#include <unordered_map>
#include <mutex>

namespace ecids_core {

class DataPublisher {
public:
    DataPublisher();
    ~DataPublisher();

    void add_channel(const std::string& name, const std::string& endpoint, int sndhwm = 10);
    void start();
    void stop();

    void publish_json(const std::string& channel,
                      const std::string& payload_json);
    void publish_binary(const std::string& channel,
                        const std::string& header_json,
                        const uint8_t* data, size_t size);

private:
    void* zmq_ctx_ = nullptr;
    std::unordered_map<std::string, void*> sockets_;
    std::mutex mutex_;
    bool started_ = false;
};

} // namespace ecids_core

#endif // ECIDS_CORE_API3_DATAPUBLISHER_H
