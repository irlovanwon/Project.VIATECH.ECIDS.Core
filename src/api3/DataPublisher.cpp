/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: API3b DataPublisher implementation
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented
 */

#include "ecids_core/api3/DataPublisher.h"
#include "ecids_core/common/Logger.h"
#include "ecids_core/common/Timestamp.h"

#include <zmq.h>
#include <nlohmann/json.hpp>

namespace ecids_core {

using json = nlohmann::json;

DataPublisher::DataPublisher() {
    zmq_ctx_ = zmq_ctx_new();
}

DataPublisher::~DataPublisher() {
    stop();
    if (zmq_ctx_) zmq_ctx_destroy(zmq_ctx_);
}

void DataPublisher::add_channel(const std::string& name, const std::string& endpoint, int sndhwm) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sockets_.count(name)) return;

    void* sock = zmq_socket(zmq_ctx_, ZMQ_PUB);
    if (!sock) {
        Logger::error("DataPublisher: zmq_socket for " + name + " failed");
        return;
    }
    zmq_setsockopt(sock, ZMQ_SNDHWM, &sndhwm, sizeof(sndhwm));
    int linger = 100;
    zmq_setsockopt(sock, ZMQ_LINGER, &linger, sizeof(linger));

    if (zmq_bind(sock, endpoint.c_str()) != 0) {
        Logger::error("DataPublisher: bind " + endpoint + " failed");
        zmq_close(sock);
        return;
    }

    sockets_[name] = sock;
    Logger::info("DataPublisher[" + name + "]: bound to " + endpoint);
}

void DataPublisher::start() {
    started_ = true;
    Logger::info("DataPublisher: started");
}

void DataPublisher::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& kv : sockets_) {
        if (kv.second) zmq_close(kv.second);
    }
    sockets_.clear();
    started_ = false;
    Logger::info("DataPublisher: stopped");
}

void DataPublisher::publish_json(const std::string& channel,
                                 const std::string& payload_json) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sockets_.find(channel);
    if (it == sockets_.end() || !it->second) return;

    json header;
    header["channel"] = channel;
    header["ts_sec"] = Timestamp::now_unix_ms() / 1000;
    header["ts_nsec"] = (Timestamp::now_unix_ms() % 1000) * 1000000;
    std::string hdr = header.dump();

    zmq_send(it->second, hdr.c_str(), hdr.size(), ZMQ_SNDMORE);
    zmq_send(it->second, payload_json.c_str(), payload_json.size(), 0);
}

void DataPublisher::publish_binary(const std::string& channel,
                                   const std::string& header_json,
                                   const uint8_t* data, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sockets_.find(channel);
    if (it == sockets_.end() || !it->second) return;

    zmq_send(it->second, header_json.c_str(), header_json.size(), ZMQ_SNDMORE);
    zmq_send(it->second, data, size, 0);
}

} // namespace ecids_core
