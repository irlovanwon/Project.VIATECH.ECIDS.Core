/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Detection to gap measurement pipeline — full algorithm integration
 * Date: 2026-06-18
 * Modification: 2026-07-02 Integrated cleat classification, 4-edge fitting, up/down gap lines,
 *               global shift disparity, same gap checking, depth reference, annotated images
 */

#ifndef ECIDS_CORE_POSTPROCESS_POSTPROCESSMODULE_H
#define ECIDS_CORE_POSTPROCESS_POSTPROCESSMODULE_H

#pragma once

#include "ecids_core/common/Types.h"
#include "ecids_core/postprocess/DetectionExtractor.h"
#include "ecids_core/postprocess/EdgeFitter.h"
#include "ecids_core/postprocess/GapDistance.h"
#include "ecids_core/postprocess/CleatClassifier.h"
#include "ecids_core/postprocess/RegionLocalizer.h"
#include "ecids_core/postprocess/SameGapChecker.h"

#include <string>
#include <functional>

namespace ecids_core {

class RecordManager;
class DatabaseWriter;

struct PostprocessConfig {
    double min_confidence = 0.5;
    double gap_filter_margin = 0.10;
    SameGapConfig same_gap_config;
    double depth_reference_mm = 0.0;
    double offset_mm = 0.0;
    double edge_inlier_threshold = 5.0;
};

class PostprocessModule {
public:
    void init(double min_confidence, double baseline_mm, double focal_length_px);
    void set_record_manager(RecordManager* mgr) { record_mgr_ = mgr; }
    void set_db_writer(DatabaseWriter* writer) { db_writer_ = writer; }
    void set_config(const PostprocessConfig& cfg) { config_ = cfg; }

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
    CleatClassifier cleat_classifier_;
    RegionLocalizer region_localizer_;
    SameGapChecker same_gap_checker_;
    EdgeFitter edge_fitter_;
    RecordManager* record_mgr_ = nullptr;
    DatabaseWriter* db_writer_ = nullptr;
    double min_confidence_ = 0.5;
    PostprocessConfig config_;

    void run_gap_pipeline_(InspectionResult& result,
                           const ExtractedDetections& extracted,
                           const uint8_t* left_data, size_t left_size,
                           const uint8_t* right_data, size_t right_size,
                           int image_height);

    std::vector<uint8_t> annotate_image_(const uint8_t* data, size_t size,
                                         const InspectionResult& result,
                                         bool stereo_annotations_only = false);

    std::vector<uint8_t> annotate_image_ai_only_(const uint8_t* data, size_t size,
                                                  const InspectionResult& result);
};

} // namespace ecids_core

#endif // ECIDS_CORE_POSTPROCESS_POSTPROCESSMODULE_H
