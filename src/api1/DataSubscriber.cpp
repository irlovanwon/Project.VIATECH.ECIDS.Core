/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: API1a DataSubscriber implementation
 * Date: 2026-06-18
 * Modification: 2026-06-24 Handle SC grouped multipart format (channel_key + JSON header + data payloads)
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

    while (running_) {
        int events;
        size_t events_size = sizeof(events);
        zmq_getsockopt(sock, ZMQ_EVENTS, &events, &events_size);
        if (!(events & ZMQ_POLLIN)) {
            zmq_pollitem_t item;
            item.socket = sock;
            item.events = ZMQ_POLLIN;
            int prc = zmq_poll(&item, 1, 200);
            if (prc <= 0) continue;
        }

        std::vector<std::vector<uint8_t>> parts;
        while (true) {
            zmq_msg_t msg;
            zmq_msg_init(&msg);
            int rc = zmq_msg_recv(&msg, sock, 0);
            if (rc < 0) {
                zmq_msg_close(&msg);
                break;
            }
            auto* d = static_cast<const uint8_t*>(zmq_msg_data(&msg));
            size_t sz = zmq_msg_size(&msg);
            parts.emplace_back(d, d + sz);
            zmq_msg_close(&msg);

            int more = 0;
            size_t more_sz = sizeof(more);
            zmq_getsockopt(sock, ZMQ_RCVMORE, &more, &more_sz);
            if (!more) break;
        }

        if (parts.empty()) continue;

        size_t hdr_idx = 0;
        size_t data_offset = 1;

        if (parts.size() >= 3) {
            json test_hdr;
            bool part0_json = false;
            try {
                test_hdr = json::parse(std::string(parts[0].begin(), parts[0].end()));
                part0_json = test_hdr.is_object();
            } catch (...) {}

            if (!part0_json) {
                hdr_idx = 1;
                data_offset = 2;
            }
        }

        json hdr;
        bool hdr_ok = false;
        if (hdr_idx < parts.size()) {
            try {
                hdr = json::parse(std::string(parts[hdr_idx].begin(), parts[hdr_idx].end()));
                hdr_ok = hdr.is_object();
            } catch (...) {}
        }

        if (!hdr_ok) {
            if (callback_) {
                DataBundle bundle;
                bundle.header.channel = channel;
                if (data_offset < parts.size()) {
                    bundle.data = std::make_shared<std::vector<uint8_t>>(std::move(parts[data_offset]));
                } else {
                    bundle.data = std::make_shared<std::vector<uint8_t>>();
                }
                callback_(channel, bundle);
            }
            continue;
        }

        int64_t ts_sec = hdr.value("ts_sec", 0LL);
        int64_t ts_nsec = hdr.value("ts_nsec", 0LL);
        uint64_t pair_id = hdr.value("pair_id", 0ULL);
        if (pair_id == 0) {
            pair_id = static_cast<uint64_t>(ts_sec) * 1000000000ULL + static_cast<uint64_t>(ts_nsec);
        }

        if (hdr.contains("parts") && hdr["parts"].is_array()) {
            auto& parts_arr = hdr["parts"];
            for (size_t i = 0; i < parts_arr.size() && data_offset + i < parts.size(); i++) {
                std::string data_id = parts_arr[i].value("id", "");

                DataBundle bundle;
                bundle.header.ts_sec = ts_sec;
                bundle.header.ts_nsec = ts_nsec;
                bundle.header.pair_id = pair_id;
                bundle.header.part = data_id;
                bundle.header.channel = hdr.value("channel", channel);
                bundle.data = std::make_shared<std::vector<uint8_t>>(std::move(parts[data_offset + i]));

                if (callback_) {
                    callback_(data_id.empty() ? channel : data_id, bundle);
                }
            }
        } else {
            DataBundle bundle;
            bundle.header.type = hdr.value("type", 0);
            bundle.header.ts_sec = ts_sec;
            bundle.header.ts_nsec = ts_nsec;
            bundle.header.pair_id = pair_id;
            bundle.header.part = hdr.value("part", "");
            bundle.header.channel = hdr.value("channel", channel);
            if (data_offset < parts.size()) {
                bundle.data = std::make_shared<std::vector<uint8_t>>(std::move(parts[data_offset]));
            } else {
                bundle.data = std::make_shared<std::vector<uint8_t>>();
            }
            if (callback_) {
                callback_(bundle.header.part.empty() ? channel : bundle.header.part, bundle);
            }
        }
    }

    zmq_close(sock);
    Logger::info("DataSubscriber[" + channel + "]: thread exited");
}

} // namespace ecids_core
