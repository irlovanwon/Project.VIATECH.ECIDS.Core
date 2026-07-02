/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: SPSC database writer — async file saves via dedicated thread
 * Date: 2026-07-02
 */

#ifndef ECIDS_CORE_DATABASE_DATABASEWRITER_H
#define ECIDS_CORE_DATABASE_DATABASEWRITER_H

#pragma once

#include "ecids_core/common/Types.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <string>

namespace ecids_core {

class RecordManager;

class DatabaseWriter {
public:
    DatabaseWriter();
    ~DatabaseWriter();

    void init(RecordManager* mgr, size_t max_queue = 200);
    void start();
    void stop();

    void enqueue_image(const std::string& record_path,
                       const std::string& subfolder,
                       const std::string& camera_id,
                       int pair_index,
                       std::shared_ptr<std::vector<uint8_t>> data,
                       const std::string& format = "jpg");

    void enqueue_ai_result(const std::string& record_path,
                           const std::string& subfolder,
                           const std::string& camera_id,
                           int pair_index,
                           const std::string& json_str);

    void enqueue_stereo_result(const std::string& record_path,
                               const std::string& subfolder,
                               const std::string& camera_id,
                               int pair_index,
                               const std::string& json_str);

    size_t queue_size() const { return queue_.size(); }

private:
    void writer_loop_();
    void process_request_(const DBWriteRequest& req);

    RecordManager* record_mgr_ = nullptr;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::queue<DBWriteRequest> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    size_t max_queue_ = 200;
};

} // namespace ecids_core

#endif // ECIDS_CORE_DATABASE_DATABASEWRITER_H
