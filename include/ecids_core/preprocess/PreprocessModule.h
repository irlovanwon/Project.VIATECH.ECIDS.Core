/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Image to AI pipeline orchestration
 * Date: 2026-06-18
 * Modification: 2026-06-21 Redesigned for full pipeline with background processing
 */

#ifndef ECIDS_CORE_PREPROCESS_PREPROCESSMODULE_H
#define ECIDS_CORE_PREPROCESS_PREPROCESSMODULE_H

#pragma once

#include "ecids_core/common/Types.h"
#include "ecids_core/preprocess/ImagePackager.h"

#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <mutex>
#include <unordered_map>

namespace ecids_core {

class DataBuffer;
class DetectionDealer;
class RecordManager;

class PreprocessModule {
public:
    struct PendingRequest {
        std::string transaction_id;
        DataBundle left;
        DataBundle right;
    };

    using ResultCallback = std::function<void(const std::string& transaction_id,
                                              const DetectionResponse& response,
                                              const DataBundle& left,
                                              const DataBundle& right)>;

    PreprocessModule();
    ~PreprocessModule();

    void init(DataBuffer* buffer, DetectionDealer* dealer,
              RecordManager* record_mgr, AIMode ai_mode);
    void set_result_callback(ResultCallback cb);

    void start_inspection(const std::string& record_path);
    void stop_inspection();

    void start_ai_test(const std::string& test_data_path);
    void stop_ai_test();

    void stop();

private:
    void inspection_loop_();
    void ai_test_loop_();
    void process_pair_(const DataBundle& left, const DataBundle& right);
    void process_file_(const std::string& filepath);
    void on_ai_result_(const DetectionResponse& response);

    DataBuffer* buffer_ = nullptr;
    DetectionDealer* dealer_ = nullptr;
    RecordManager* record_mgr_ = nullptr;
    AIMode ai_mode_ = AIMode::File;
    ResultCallback callback_;

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> ai_test_mode_{false};
    std::string active_record_path_;
    std::string test_data_path_;

    std::mutex pending_mutex_;
    std::unordered_map<std::string, PendingRequest> pending_;
};

} // namespace ecids_core

#endif // ECIDS_CORE_PREPROCESS_PREPROCESSMODULE_H
