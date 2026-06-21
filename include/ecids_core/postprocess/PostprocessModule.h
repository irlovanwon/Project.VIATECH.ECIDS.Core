/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Detection to gap measurement pipeline
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented full postprocessing pipeline
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
                             const uint8_t* right_data, size_t right_size);

    InspectionResult process_ai_test(const DetectionResponse& response);

private:
    DetectionExtractor extractor_;
    GapDistance gap_dist_;
    RecordManager* record_mgr_ = nullptr;
    double min_confidence_ = 0.5;
};

} // namespace ecids_core

#endif // ECIDS_CORE_POSTPROCESS_POSTPROCESSMODULE_H
