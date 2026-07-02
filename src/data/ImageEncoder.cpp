/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: SPSC image encoder — JPG encoding via libjpeg-turbo for WSS
 * Date: 2026-06-26
 * Modification: 2026-07-02 Switched to libjpeg-turbo TurboJPEG API, JPG-only for WSS
 */

#include "ecids_core/data/ImageEncoder.h"
#include "ecids_core/common/Logger.h"

#include <turbojpeg.h>

namespace ecids_core {

ImageEncoder::ImageEncoder() {}

ImageEncoder::~ImageEncoder() {
    stop();
}

void ImageEncoder::start() {
    if (running_.load()) return;
    running_.store(true);
    thread_ = std::thread(&ImageEncoder::encode_loop_, this);
    Logger::info("ImageEncoder: started (libjpeg-turbo, quality=" + std::to_string(quality_) + ")");
}

void ImageEncoder::stop() {
    if (!running_.load()) return;
    running_.store(false);
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
    Logger::info("ImageEncoder: stopped");
}

void ImageEncoder::enqueue(const uint8_t* data, size_t size, int width, int height,
                           const std::string& topic, const std::string& header_json) {
    if (!running_.load() || !data || size == 0) return;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= MAX_QUEUE) {
            queue_.pop();
        }
        EncodeRequest req;
        req.raw_data.assign(data, data + size);
        req.width = width;
        req.height = height;
        req.topic = topic;
        req.header_json = header_json;
        queue_.push(std::move(req));
    }
    cv_.notify_one();
}

bool ImageEncoder::encode_jpeg_turbo_(const uint8_t* bgra, int w, int h,
                                       std::vector<uint8_t>& out) {
    tjhandle handle = tjInitCompress();
    if (!handle) return false;

    unsigned char* jpegBuf = nullptr;
    unsigned long jpegSize = 0;

    int rc = tjCompress2(handle, const_cast<uint8_t*>(bgra),
                         w, 0, h, TJPF_BGRA,
                         &jpegBuf, &jpegSize,
                         TJSAMP_420, quality_, TJFLAG_FASTDCT);

    if (rc == 0 && jpegBuf && jpegSize > 0) {
        out.assign(jpegBuf, jpegBuf + jpegSize);
    }

    if (jpegBuf) tjFree(jpegBuf);
    tjDestroy(handle);

    return (rc == 0 && !out.empty());
}

void ImageEncoder::encode_loop_() {
    while (running_.load()) {
        EncodeRequest req;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, std::chrono::milliseconds(200), [this] {
                return !queue_.empty() || !running_.load();
            });
            if (!running_.load() && queue_.empty()) break;
            if (queue_.empty()) continue;
            req = std::move(queue_.front());
            queue_.pop();
        }

        int h = req.height;
        int w = req.width;
        if (w <= 0 || h <= 0) {
            if (req.raw_data.size() % 4 == 0) {
                w = 1920;
                h = static_cast<int>(req.raw_data.size() / (static_cast<size_t>(w) * 4));
            }
        }
        if (w <= 0 || h <= 0 ||
            req.raw_data.size() < static_cast<size_t>(w) * h * 4) {
            continue;
        }

        EncodeResult result;
        result.topic = req.topic;
        result.header_json = req.header_json;

        if (!encode_jpeg_turbo_(req.raw_data.data(), w, h, result.jpg_data)) {
            Logger::warn("ImageEncoder: tjCompress2 failed, skipping frame");
            continue;
        }

        if (callback_) callback_(result);
    }
}

} // namespace ecids_core
