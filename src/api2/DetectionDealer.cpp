/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: API2a DetectionDealer implementation
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented
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

void DetectionDealer::init(const std::string& endpoint, const std::string& identity,
                            int sndhwm, int rcvhwm, int poll_timeout_ms) {
    endpoint_ = endpoint;
    identity_ = identity;
    sndhwm_ = sndhwm;
    rcvhwm_ = rcvhwm;
    poll_timeout_ms_ = poll_timeout_ms;
}

void DetectionDealer::start() {
    if (running_) return;

    sock_ = zmq_socket(zmq_ctx_, ZMQ_DEALER);
    if (!sock_) {
        Logger::error("DetectionDealer: zmq_socket failed");
        return;
    }

    zmq_setsockopt(sock_, ZMQ_IDENTITY, identity_.c_str(), identity_.size());
    zmq_setsockopt(sock_, ZMQ_SNDHWM, &sndhwm_, sizeof(sndhwm_));
    zmq_setsockopt(sock_, ZMQ_RCVHWM, &rcvhwm_, sizeof(rcvhwm_));
    int linger = 100;
    zmq_setsockopt(sock_, ZMQ_LINGER, &linger, sizeof(linger));

    if (zmq_connect(sock_, endpoint_.c_str()) != 0) {
        Logger::error("DetectionDealer: connect " + endpoint_ + " failed");
        zmq_close(sock_);
        sock_ = nullptr;
        return;
    }

    running_ = true;
    thread_ = std::thread(&DetectionDealer::poll_loop_, this);
    Logger::info("DetectionDealer: connected to " + endpoint_);
}

void DetectionDealer::stop() {
    if (!running_) return;
    running_ = false;
    if (thread_.joinable()) thread_.join();
    if (sock_) {
        zmq_close(sock_);
        sock_ = nullptr;
    }
    Logger::info("DetectionDealer: stopped");
}

void DetectionDealer::send_file_request(const std::string& transaction_id,
                                        const std::vector<std::string>& image_uris,
                                        const std::vector<std::string>& filenames) {
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
    zmq_send(sock_, msg.c_str(), msg.size(), 0);
}

void DetectionDealer::send_binary_request(const std::string& transaction_id,
                                          const uint8_t* left_data, size_t left_size,
                                          const uint8_t* right_data, size_t right_size) {
    json header;
    header["TransactionID"] = transaction_id;
    header["Mode"] = "Stream";
    header["DealerID"] = identity_;
    header["Timestamp"] = Timestamp::now_string().substr(0, 15);
    header["ImageCount"] = 2;
    header["Parts"] = json::array({
        json{{"Index", 0}, {"Part", "left"}, {"Resolution", "1920,1080"}, {"Format", "Mono"}, {"Size", left_size}},
        json{{"Index", 1}, {"Part", "right"}, {"Resolution", "1920,1080"}, {"Format", "Mono"}, {"Size", right_size}}
    });

    std::string hdr = header.dump();

    zmq_send(sock_, hdr.c_str(), hdr.size(), ZMQ_SNDMORE);
    zmq_send(sock_, left_data, left_size, ZMQ_SNDMORE);
    zmq_send(sock_, right_data, right_size, 0);
}

void DetectionDealer::poll_loop_() {
    while (running_) {
        zmq_pollitem_t item;
        item.socket = sock_;
        item.events = ZMQ_POLLIN;

        int rc = zmq_poll(&item, 1, poll_timeout_ms_);
        if (rc <= 0) continue;
        if (!(item.revents & ZMQ_POLLIN)) continue;

        zmq_msg_t msg;
        zmq_msg_init(&msg);

        std::vector<std::vector<uint8_t>> parts;
        bool ok = true;

        while (true) {
            if (zmq_msg_recv(&msg, sock_, 0) < 0) {
                ok = false;
                break;
            }
            auto* d = static_cast<uint8_t*>(zmq_msg_data(&msg));
            size_t s = zmq_msg_size(&msg);
            parts.emplace_back(d, d + s);

            int more;
            size_t more_size = sizeof(more);
            zmq_getsockopt(sock_, ZMQ_RCVMORE, &more, &more_size);
            if (!more) break;
        }
        zmq_msg_close(&msg);

        if (!ok || parts.empty()) continue;

        try {
            std::string hdr_str(parts[0].begin(), parts[0].end());
            json resp = json::parse(hdr_str);

            DetectionResponse dr;
            dr.transaction_id = resp.value("TransactionID", "");
            dr.dealer_id = resp.value("DealerID", "");
            dr.ts_received = resp.value("TimestampReceived", "");
            dr.ts_replied = resp.value("TimestampReplied", "");

            if (resp.contains("Result") && resp["Result"].is_array()) {
                for (const auto& det : resp["Result"]) {
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
