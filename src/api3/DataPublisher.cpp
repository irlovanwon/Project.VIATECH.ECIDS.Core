/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: API3b DataPublisher — ZMQ_IMMEDIATE=1 + zero-copy
 * Date: 2026-06-18
 * Modification: 2026-07-02 Added ZMQ_IMMEDIATE=1, zmq_msg_init_data zero-copy for binary
 */

#include "ecids_core/api3/DataPublisher.h"
#include "ecids_core/common/Logger.h"
#include "ecids_core/common/Timestamp.h"

#include <zmq.h>
#include <nlohmann/json.hpp>

namespace ecids_core {

using json = nlohmann::json;

static void zc_free_fn_(void* data, void* /*hint*/) {
    delete[] static_cast<uint8_t*>(data);
}

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

    int immediate = 1;
    zmq_setsockopt(sock, ZMQ_IMMEDIATE, &immediate, sizeof(immediate));

    int linger = 100;
    zmq_setsockopt(sock, ZMQ_LINGER, &linger, sizeof(linger));

    if (zmq_bind(sock, endpoint.c_str()) != 0) {
        Logger::error("DataPublisher: bind " + endpoint + " failed");
        zmq_close(sock);
        return;
    }

    sockets_[name] = sock;
    Logger::info("DataPublisher[" + name + "]: bound to " + endpoint + " (IMMEDIATE=1)");
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

    zmq_send(it->second, hdr.c_str(), hdr.size(), ZMQ_SNDMORE | ZMQ_DONTWAIT);
    zmq_send(it->second, payload_json.c_str(), payload_json.size(), ZMQ_DONTWAIT);
}

void DataPublisher::publish_binary(const std::string& channel,
                                   const std::string& header_json,
                                   const uint8_t* data, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sockets_.find(channel);
    if (it == sockets_.end() || !it->second) return;

    zmq_send(it->second, header_json.c_str(), header_json.size(), ZMQ_SNDMORE | ZMQ_DONTWAIT);

    uint8_t* buf = new uint8_t[size];
    memcpy(buf, data, size);

    zmq_msg_t msg;
    zmq_msg_init_data(&msg, buf, size, zc_free_fn_, nullptr);
    zmq_msg_send(&msg, it->second, ZMQ_DONTWAIT);
}

} // namespace ecids_core
