/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: SPSC database writer — async file saves via dedicated thread
 * Date: 2026-07-02
 */

#include "ecids_core/database/DatabaseWriter.h"
#include "ecids_core/database/RecordManager.h"
#include "ecids_core/common/Logger.h"

namespace ecids_core {

DatabaseWriter::DatabaseWriter() {}

DatabaseWriter::~DatabaseWriter() {
    stop();
}

void DatabaseWriter::init(RecordManager* mgr, size_t max_queue) {
    record_mgr_ = mgr;
    max_queue_ = max_queue;
}

void DatabaseWriter::start() {
    if (running_.load()) return;
    running_.store(true);
    thread_ = std::thread(&DatabaseWriter::writer_loop_, this);
    Logger::info("DatabaseWriter: started (max_queue=" + std::to_string(max_queue_) + ")");
}

void DatabaseWriter::stop() {
    if (!running_.load()) return;
    running_.store(false);
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
    Logger::info("DatabaseWriter: stopped");
}

void DatabaseWriter::enqueue_image(const std::string& record_path,
                                   const std::string& subfolder,
                                   const std::string& camera_id,
                                   int pair_index,
                                   std::shared_ptr<std::vector<uint8_t>> data,
                                   const std::string& format) {
    if (!running_.load() || !data || data->empty()) return;

    DBWriteRequest req;
    req.type = DBWriteRequest::Type::Image;
    req.record_path = record_path;
    req.subfolder = subfolder;
    req.camera_id = camera_id;
    req.pair_index = pair_index;
    req.data = data;
    req.format = format;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= max_queue_) {
            queue_.pop();
        }
        queue_.push(std::move(req));
    }
    cv_.notify_one();
}

void DatabaseWriter::enqueue_ai_result(const std::string& record_path,
                                       const std::string& subfolder,
                                       const std::string& camera_id,
                                       int pair_index,
                                       const std::string& json_str) {
    if (!running_.load()) return;

    DBWriteRequest req;
    req.type = DBWriteRequest::Type::AIResult;
    req.record_path = record_path;
    req.subfolder = subfolder;
    req.camera_id = camera_id;
    req.pair_index = pair_index;
    req.text_data = json_str;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= max_queue_) {
            queue_.pop();
        }
        queue_.push(std::move(req));
    }
    cv_.notify_one();
}

void DatabaseWriter::enqueue_stereo_result(const std::string& record_path,
                                           const std::string& subfolder,
                                           const std::string& camera_id,
                                           int pair_index,
                                           const std::string& json_str) {
    if (!running_.load()) return;

    DBWriteRequest req;
    req.type = DBWriteRequest::Type::StereoResult;
    req.record_path = record_path;
    req.subfolder = subfolder;
    req.camera_id = camera_id;
    req.pair_index = pair_index;
    req.text_data = json_str;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= max_queue_) {
            queue_.pop();
        }
        queue_.push(std::move(req));
    }
    cv_.notify_one();
}

void DatabaseWriter::writer_loop_() {
    while (running_.load()) {
        DBWriteRequest req;
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
        process_request_(req);
    }
}

void DatabaseWriter::process_request_(const DBWriteRequest& req) {
    if (!record_mgr_) return;

    switch (req.type) {
        case DBWriteRequest::Type::Image:
            if (req.data && !req.data->empty()) {
                record_mgr_->save_image(req.record_path, req.subfolder,
                    req.camera_id, req.pair_index,
                    req.data->data(), req.data->size(), req.format);
            }
            break;
        case DBWriteRequest::Type::AIResult:
            record_mgr_->save_ai_result(req.record_path, req.subfolder,
                req.camera_id, req.pair_index, req.text_data);
            break;
        case DBWriteRequest::Type::StereoResult:
            record_mgr_->save_stereo_result(req.record_path, req.subfolder,
                req.camera_id, req.pair_index, req.text_data);
            break;
        case DBWriteRequest::Type::PointCloud:
            if (req.data && !req.data->empty()) {
                record_mgr_->save_pointcloud(req.record_path, req.subfolder,
                    req.pair_index, req.data->data(), req.data->size());
            }
            break;
        case DBWriteRequest::Type::DepthMap:
            if (req.data && !req.data->empty()) {
                record_mgr_->save_depth(req.record_path, req.subfolder,
                    req.pair_index, req.data->data(), req.data->size());
            }
            break;
    }
}

} // namespace ecids_core
