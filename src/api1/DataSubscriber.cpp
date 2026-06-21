/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: API1a DataSubscriber implementation
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented
 */

#include "ecids_core/api1/DataSubscriber.h"
#include "ecids_core/common/Logger.h"

#include <zmq.h>
#include <nlohmann/json.hpp>
#include <cstring>

namespace ecids_core {

using json = nlohmann::json;

DataSubscriber::DataSubscriber() {
    zmq_ctx_ = zmq_ctx_new();
}

DataSubscriber::~DataSubscriber() {
    stop();
    if (zmq_ctx_) zmq_ctx_destroy(zmq_ctx_);
}

void DataSubscriber::add_channel(const std::string& name, const std::string& endpoint) {
    channels_.push_back({name, endpoint});
}

void DataSubscriber::set_callback(DataCallback cb) {
    callback_ = std::move(cb);
}

void DataSubscriber::start() {
    if (running_) return;
    running_ = true;

    for (const auto& ch : channels_) {
        threads_.emplace_back(&DataSubscriber::subscriber_loop_, this, ch.name, ch.endpoint);
    }
    Logger::info("DataSubscriber: started " + std::to_string(channels_.size()) + " channels");
}

void DataSubscriber::stop() {
    if (!running_) return;
    running_ = false;

    for (auto& t : threads_) {
        if (t.joinable()) t.join();
    }
    threads_.clear();
    Logger::info("DataSubscriber: stopped");
}

void DataSubscriber::subscriber_loop_(const std::string& channel, const std::string& endpoint) {
    void* sock = zmq_socket(zmq_ctx_, ZMQ_SUB);
    if (!sock) {
        Logger::error("DataSubscriber[" + channel + "]: zmq_socket failed");
        return;
    }

    zmq_setsockopt(sock, ZMQ_SUBSCRIBE, "", 0);
    int linger = 100;
    zmq_setsockopt(sock, ZMQ_LINGER, &linger, sizeof(linger));

    int rc = zmq_connect(sock, endpoint.c_str());
    if (rc != 0) {
        Logger::error("DataSubscriber[" + channel + "]: connect " + endpoint + " failed");
        zmq_close(sock);
        return;
    }

    Logger::info("DataSubscriber[" + channel + "]: connected to " + endpoint);

    zmq_msg_t msg_header;
    zmq_msg_t msg_body;

    while (running_) {
        zmq_msg_init(&msg_header);
        zmq_msg_init(&msg_body);

        int events;
        size_t events_size = sizeof(events);
        zmq_getsockopt(sock, ZMQ_EVENTS, &events, &events_size);
        if (!(events & ZMQ_POLLIN)) {
            zmq_pollitem_t item;
            item.socket = sock;
            item.events = ZMQ_POLLIN;
            int prc = zmq_poll(&item, 1, 200);
            if (prc <= 0) {
                zmq_msg_close(&msg_header);
                zmq_msg_close(&msg_body);
                continue;
            }
        }

        int rc1 = zmq_msg_recv(&msg_header, sock, 0);
        if (rc1 < 0) {
            zmq_msg_close(&msg_header);
            zmq_msg_close(&msg_body);
            continue;
        }

        int more;
        size_t more_size = sizeof(more);
        zmq_getsockopt(sock, ZMQ_RCVMORE, &more, &more_size);

        if (more) {
            zmq_msg_recv(&msg_body, sock, 0);
        }

        DataBundle bundle;

        auto* hdr_data = static_cast<char*>(zmq_msg_data(&msg_header));
        size_t hdr_size = zmq_msg_size(&msg_header);

        if (hdr_size > 0) {
            try {
                json hdr = json::parse(std::string(hdr_data, hdr_size));
                bundle.header.type = hdr.value("type", 0);
                bundle.header.ts_sec = hdr.value("ts_sec", 0LL);
                bundle.header.ts_nsec = hdr.value("ts_nsec", 0LL);
                bundle.header.pair_id = hdr.value("pair_id", 0ULL);
                bundle.header.part = hdr.value("part", "");
                bundle.header.channel = hdr.value("channel", channel);
            } catch (const std::exception& e) {
                bundle.header.channel = channel;
            }
        }

        auto* body_data = static_cast<uint8_t*>(zmq_msg_data(&msg_body));
        size_t body_size = zmq_msg_size(&msg_body);
        if (body_size > 0) {
            bundle.data = std::make_shared<std::vector<uint8_t>>(body_data, body_data + body_size);
        } else {
            bundle.data = std::make_shared<std::vector<uint8_t>>();
        }

        zmq_msg_close(&msg_header);
        zmq_msg_close(&msg_body);

        if (callback_) {
            callback_(channel, bundle);
        }
    }

    zmq_close(sock);
    Logger::info("DataSubscriber[" + channel + "]: thread exited");
}

} // namespace ecids_core
