/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: API2a DetectionDealer implementation (PUB/SUB)
 * Date: 2026-06-18
 * Modification: 2026-06-23 Refactored DEALER→PUB+SUB, added zmq_msg_init_data zero-copy
 */

#include "ecids_core/api2/DetectionDealer.h"
#include "ecids_core/common/Logger.h"
#include "ecids_core/common/Timestamp.h"

#include <zmq.h>
#include <nlohmann/json.hpp>
#include <cstring>
#include <sstream>

namespace ecids_core {

using json = nlohmann::json;

DetectionDealer::DetectionDealer() {
    zmq_ctx_ = zmq_ctx_new();
}

DetectionDealer::~DetectionDealer() {
    stop();
    if (zmq_ctx_) zmq_ctx_destroy(zmq_ctx_);
}

void DetectionDealer::init(const std::string& pub_endpoint, const std::string& sub_endpoint,
                            const std::string& identity,
                            int sndhwm, int rcvhwm, int poll_timeout_ms) {
    pub_endpoint_ = pub_endpoint;
    sub_endpoint_ = sub_endpoint;
    identity_ = identity;
    sndhwm_ = sndhwm;
    rcvhwm_ = rcvhwm;
    poll_timeout_ms_ = poll_timeout_ms;
}

void DetectionDealer::start() {
    if (running_) return;

    pub_sock_ = zmq_socket(zmq_ctx_, ZMQ_PUB);
    if (!pub_sock_) {
        Logger::error("DetectionDealer: PUB socket creation failed");
        return;
    }
    zmq_setsockopt(pub_sock_, ZMQ_SNDHWM, &sndhwm_, sizeof(sndhwm_));
    int linger = 100;
    zmq_setsockopt(pub_sock_, ZMQ_LINGER, &linger, sizeof(linger));

    if (zmq_bind(pub_sock_, pub_endpoint_.c_str()) != 0) {
        Logger::error("DetectionDealer: PUB bind " + pub_endpoint_ + " failed");
        zmq_close(pub_sock_);
        pub_sock_ = nullptr;
        return;
    }

    sub_sock_ = zmq_socket(zmq_ctx_, ZMQ_SUB);
    if (!sub_sock_) {
        Logger::error("DetectionDealer: SUB socket creation failed");
        zmq_close(pub_sock_);
        pub_sock_ = nullptr;
        return;
    }
    zmq_setsockopt(sub_sock_, ZMQ_RCVHWM, &rcvhwm_, sizeof(rcvhwm_));
    zmq_setsockopt(sub_sock_, ZMQ_LINGER, &linger, sizeof(linger));
    zmq_setsockopt(sub_sock_, ZMQ_SUBSCRIBE, "", 0);

    if (zmq_connect(sub_sock_, sub_endpoint_.c_str()) != 0) {
        Logger::error("DetectionDealer: SUB connect " + sub_endpoint_ + " failed");
        zmq_close(pub_sock_);
        zmq_close(sub_sock_);
        pub_sock_ = nullptr;
        sub_sock_ = nullptr;
        return;
    }

    running_ = true;
    thread_ = std::thread(&DetectionDealer::poll_loop_, this);
    Logger::info("DetectionDealer: PUB bind " + pub_endpoint_ + " SUB connect←" + sub_endpoint_);
}

void DetectionDealer::stop() {
    if (!running_) return;
    running_ = false;
    if (thread_.joinable()) thread_.join();
    if (pub_sock_) {
        zmq_close(pub_sock_);
        pub_sock_ = nullptr;
    }
    if (sub_sock_) {
        zmq_close(sub_sock_);
        sub_sock_ = nullptr;
    }
    Logger::info("DetectionDealer: stopped");
}

static void zmq_send_zero_copy(void* sock, const void* data, size_t size, int flags) {
    zmq_msg_t msg;
    zmq_msg_init_size(&msg, size);
    memcpy(zmq_msg_data(&msg), data, size);
    zmq_msg_send(&msg, sock, flags);
}

void DetectionDealer::send_file_request(const std::string& transaction_id,
                                        const std::vector<std::string>& image_uris,
                                        const std::vector<std::string>& filenames) {
    if (!pub_sock_) return;

    json req;
    req["TransactionID"] = transaction_id;
    req["Mode"] = "File";
    req["DealerID"] = identity_;
    req["Timestamp"] = Timestamp::now_string().substr(0, 15);

    json data_arr = json::array();
    for (size_t i = 0; i < image_uris.size(); ++i) {
        json item;
        item["URI"] = image_uris[i];
        item["FileName"] = (i < filenames.size()) ? filenames[i] : "";
        item["Resolution"] = "1920,1080";
        item["Format"] = "Mono";
        data_arr.push_back(item);
    }
    req["Data"] = data_arr;

    std::string msg = req.dump();
    zmq_send_zero_copy(pub_sock_, msg.c_str(), msg.size(), ZMQ_DONTWAIT);
}

void DetectionDealer::send_binary_request(const std::string& transaction_id,
                                          const uint8_t* left_data, size_t left_size,
                                          const uint8_t* right_data, size_t right_size) {
    if (!pub_sock_) return;

    json header;
    header["TransactionID"] = transaction_id;
    header["Mode"] = "Binary";
    header["DealerID"] = identity_;
    header["Timestamp"] = Timestamp::now_string().substr(0, 15);
    header["ImageCount"] = 1;
    header["Parts"] = json::array({
        json{{"Index", 0}, {"Part", "left"}, {"Resolution", "1920,1080"}, {"Format", "Mono"}, {"Size", left_size}}
    });
    header["Data"] = json::array({
        json{{"FileName", "L.jpg"}, {"Format", "Mono"}, {"Resolution", "1920,1080"}}
    });

    std::string hdr = header.dump();

    zmq_send_zero_copy(pub_sock_, hdr.c_str(), hdr.size(), ZMQ_SNDMORE | ZMQ_DONTWAIT);
    zmq_send_zero_copy(pub_sock_, left_data, left_size, ZMQ_DONTWAIT);
}

void DetectionDealer::poll_loop_() {
    while (running_) {
        zmq_pollitem_t item;
        item.socket = sub_sock_;
        item.events = ZMQ_POLLIN;

        int rc = zmq_poll(&item, 1, poll_timeout_ms_);
        if (rc <= 0) continue;
        if (!(item.revents & ZMQ_POLLIN)) continue;

        zmq_msg_t msg;
        zmq_msg_init(&msg);

        std::vector<std::vector<uint8_t>> parts;
        bool ok = true;

        while (true) {
            if (zmq_msg_recv(&msg, sub_sock_, 0) < 0) {
                ok = false;
                break;
            }
            auto* d = static_cast<uint8_t*>(zmq_msg_data(&msg));
            size_t s = zmq_msg_size(&msg);
            parts.emplace_back(d, d + s);

            int more;
            size_t more_size = sizeof(more);
            zmq_getsockopt(sub_sock_, ZMQ_RCVMORE, &more, &more_size);
            if (!more) break;
        }
        zmq_msg_close(&msg);

        if (!ok || parts.empty()) continue;

        try {
            std::string hdr_str(parts[0].begin(), parts[0].end());
            json resp = json::parse(hdr_str);

            DetectionResponse dr;
            dr.transaction_id = resp.value("TransactionID", resp.value("transaction_id", ""));
            dr.dealer_id = resp.value("DealerID", resp.value("dealer_id", ""));
            dr.ts_received = resp.value("TimestampReceived", "");
            dr.ts_replied = resp.value("TimestampReplied", "");

            json result_array;
            if (resp.contains("Result") && resp["Result"].is_array()) {
                result_array = resp["Result"];
            } else if (parts.size() > 1) {
                std::string body_str(parts[1].begin(), parts[1].end());
                json body = json::parse(body_str);
                // Check for service restart notification from AIVD
                if (body.contains("Type") && body["Type"] == "Reconnect") {
                    Logger::info("DetectionDealer: AIVD reconnect notification received");
                    if (reconnect_cb_) reconnect_cb_();
                    continue;
                }
                if (body.contains("TransactionID"))
                    dr.transaction_id = body.value("TransactionID", dr.transaction_id);
                if (body.contains("Result") && body["Result"].is_array()) {
                    result_array = body["Result"];
                }
            }

            if (!result_array.empty()) {
                for (const auto& det : result_array) {
                    Detection d;
                    d.label_id = det.value("LabelID", "");
                    d.confidence = std::stod(det.value("Confidence", "0.0"));
                    d.file_name = det.value("FileName", "");
                    d.ts_start = det.value("TimestampStart", "");
                    d.ts_end = det.value("TimestampEnd", "");

                    std::string coords = det.value("Coordinates", "");
                    if (!coords.empty()) {
                        std::istringstream ss(coords);
                        std::string token;
                        std::vector<double> nums;
                        while (std::getline(ss, token, ',')) {
                            try { nums.push_back(std::stod(token)); } catch (...) {}
                        }
                        for (size_t i = 0; i + 1 < nums.size(); i += 2) {
                            d.coordinates.emplace_back(nums[i], nums[i+1]);
                        }
                    }
                    dr.results.push_back(std::move(d));
                }
            }

            if (callback_) callback_(dr);
        } catch (const std::exception& e) {
            Logger::error(std::string("DetectionDealer: parse error: ") + e.what());
        }
    }
}

} // namespace ecids_core
