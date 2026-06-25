/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Detection to gap measurement pipeline
 * Date: 2026-06-18
 * Modification: 2026-06-23 Updated RecordManager API calls, added pair_index/sub_task params
 */

#ifndef ECIDS_CORE_POSTPROCESS_POSTPROCESSMODULE_H
#define ECIDS_CORE_POSTPROCESS_POSTPROCESSMODULE_H

#pragma once

#include "ecids_core/common/Types.h"
#include "ecids_core/postprocess/DetectionExtractor.h"
#include "ecids_core/postprocess/GapDistance.h"

#include <string>
#include <functional>

namespace ecids_core {

class RecordManager;

class PostprocessModule {
public:
    void init(double min_confidence, double baseline_mm, double focal_length_px);
    void set_record_manager(RecordManager* mgr) { record_mgr_ = mgr; }

    InspectionResult process(const DetectionResponse& response,
                             const std::string& station_id,
                             const std::string& escalator_id,
                             const std::string& task_id,
                             const uint8_t* left_data, size_t left_size,
                             const uint8_t* right_data, size_t right_size,
                             int pair_index = 0,
                             InspectionSubTask sub_task = InspectionSubTask::None,
                             double working_distance_mm = 0.0);

    InspectionResult process_ai_test(const DetectionResponse& response,
                                     const uint8_t* left_data, size_t left_size,
                                     const uint8_t* right_data, size_t right_size,
                                     int pair_index = 0);

private:
    DetectionExtractor extractor_;
    GapDistance gap_dist_;
    RecordManager* record_mgr_ = nullptr;
    double min_confidence_ = 0.5;

    std::vector<uint8_t> annotate_image_(const uint8_t* data, size_t size,
                                         const InspectionResult& result);
};

} // namespace ecids_core

#endif // ECIDS_CORE_POSTPROCESS_POSTPROCESSMODULE_H
