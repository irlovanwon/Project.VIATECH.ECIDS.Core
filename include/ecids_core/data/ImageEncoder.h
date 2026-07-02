/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: SPSC image encoder — dedicated thread for JPG encoding via libjpeg-turbo (WSS only)
 * Date: 2026-06-26
 * Modification: 2026-07-02 Changed to libjpeg-turbo, JPG-only for WSS, removed ZMQ/WebP
 */

#ifndef ECIDS_CORE_DATA_IMAGEENCODER_H
#define ECIDS_CORE_DATA_IMAGEENCODER_H

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <queue>

namespace ecids_core {

struct EncodeRequest {
    std::vector<uint8_t> raw_data;
    int width = 0;
    int height = 0;
    std::string topic;
    std::string header_json;
};

struct EncodeResult {
    std::vector<uint8_t> jpg_data;
    std::string topic;
    std::string header_json;
};

class ImageEncoder {
public:
    using ResultCallback = std::function<void(const EncodeResult&)>;

    ImageEncoder();
    ~ImageEncoder();

    void start();
    void stop();

    void set_callback(ResultCallback cb) { callback_ = std::move(cb); }
    void set_quality(int quality) { quality_ = quality; }

    void enqueue(const uint8_t* data, size_t size, int width, int height,
                 const std::string& topic, const std::string& header_json);

    size_t queue_size() const { return queue_.size(); }

private:
    void encode_loop_();
    bool encode_jpeg_turbo_(const uint8_t* bgra, int w, int h,
                             std::vector<uint8_t>& out);

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::queue<EncodeRequest> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    ResultCallback callback_;
    int quality_ = 85;
    static constexpr size_t MAX_QUEUE = 5;
};

} // namespace ecids_core

#endif // ECIDS_CORE_DATA_IMAGEENCODER_H
