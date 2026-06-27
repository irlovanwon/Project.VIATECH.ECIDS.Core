/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: SPSC image encoder implementation
 * Date: 2026-06-26
 */

#include "ecids_core/data/ImageEncoder.h"
#include "ecids_core/common/Logger.h"

#include <opencv2/opencv.hpp>

namespace ecids_core {

ImageEncoder::~ImageEncoder() {
    stop();
}

void ImageEncoder::start() {
    if (running_.load()) return;
    running_.store(true);
    thread_ = std::thread(&ImageEncoder::encode_loop_, this);
    Logger::info("ImageEncoder: started");
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

        EncodeResult result;
        result.topic = req.topic;
        result.header_json = req.header_json;

        int h = req.height;
        int w = req.width;
        if (w <= 0 || h <= 0) {
            if (req.raw_data.size() % 4 == 0) {
                w = 1920;
                h = (int)(req.raw_data.size() / (w * 4));
            }
        }
        if (w <= 0 || h <= 0 || req.raw_data.size() < (size_t)(w * h * 4)) {
            continue;
        }

        cv::Mat img(h, w, CV_8UC4, const_cast<uint8_t*>(req.raw_data.data()));
        if (img.empty()) continue;

        cv::imencode(".webp", img, result.webp_data, {cv::IMWRITE_WEBP_QUALITY, 80});
        cv::imencode(".jpg", img, result.jpg_data, {cv::IMWRITE_JPEG_QUALITY, 85});

        if (callback_) callback_(result);
    }
}

} // namespace ecids_core
