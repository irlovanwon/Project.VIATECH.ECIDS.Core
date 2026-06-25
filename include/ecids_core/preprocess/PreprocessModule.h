/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Image to AI pipeline orchestration
 * Date: 2026-06-18
 * Modification: 2026-06-23 Inspection sub-tasks, working distance, new filename convention
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
        int pair_index = 0;
        InspectionSubTask sub_task = InspectionSubTask::None;
        double working_distance_mm = 0.0;
    };

    using ResultCallback = std::function<void(const std::string& transaction_id,
                                              const DetectionResponse& response,
                                              const DataBundle& left,
                                              const DataBundle& right,
                                              int pair_index,
                                              InspectionSubTask sub_task,
                                              double working_distance_mm)>;

    using CompletionCallback = std::function<void()>;

    PreprocessModule();
    ~PreprocessModule();

    void init(DataBuffer* buffer, DetectionDealer* dealer,
              RecordManager* record_mgr, AIMode ai_mode);
    void set_result_callback(ResultCallback cb);
    void set_completion_callback(CompletionCallback cb) { completion_callback_ = std::move(cb); }
    void set_installation_fps(double fps) { installation_fps_ = fps; }

    void start_inspection(const std::string& record_path);
    void stop_inspection();

    void set_sub_task(InspectionSubTask st);
    InspectionSubTask sub_task() const { return sub_task_; }

    void start_ai_test(const std::string& test_data_path, const std::string& record_path = "");
    void stop_ai_test();

    void stop();
    void clear_pending();

private:
    void inspection_loop_();
    void ai_test_loop_();
    void process_pair_(const DataBundle& left, const DataBundle& right, double working_distance_mm = 0.0);
    void process_file_(const std::string& filepath);
    void process_pair_file_(const std::string& left_path, const std::string& right_path);
    void on_ai_result_(const DetectionResponse& response);

    std::string current_subfolder_() const;

    DataBuffer* buffer_ = nullptr;
    DetectionDealer* dealer_ = nullptr;
    RecordManager* record_mgr_ = nullptr;
    AIMode ai_mode_ = AIMode::File;
    ResultCallback callback_;
    CompletionCallback completion_callback_;
    double installation_fps_ = 1.0;

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> ai_test_mode_{false};
    std::string active_record_path_;
    std::string test_data_path_;

    std::atomic<InspectionSubTask> sub_task_{InspectionSubTask::None};
    std::atomic<int> pair_index_{0};

    std::mutex pending_mutex_;
    std::unordered_map<std::string, PendingRequest> pending_;
};

} // namespace ecids_core

#endif // ECIDS_CORE_PREPROCESS_PREPROCESSMODULE_H
