/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: PostprocessModule implementation
 * Date: 2026-06-18
 * Modification: 2026-06-23 Updated RecordManager API, added pair_index/sub_task/working_distance
 */

#include "ecids_core/postprocess/PostprocessModule.h"
#include "ecids_core/postprocess/EdgeFitter.h"
#include "ecids_core/postprocess/GapLocalizer.h"
#include "ecids_core/database/RecordManager.h"
#include "ecids_core/common/Logger.h"

#include <nlohmann/json.hpp>

namespace ecids_core {

using json = nlohmann::json;

void PostprocessModule::init(double min_confidence, double baseline_mm, double focal_length_px) {
    min_confidence_ = min_confidence;
    gap_dist_.init(baseline_mm, focal_length_px);
}

InspectionResult PostprocessModule::process(const DetectionResponse& response,
                                            const std::string& station_id,
                                            const std::string& escalator_id,
                                            const std::string& task_id,
                                            const uint8_t* left_data, size_t left_size,
                                            const uint8_t* right_data, size_t right_size,
                                            int pair_index,
                                            InspectionSubTask sub_task,
                                            double working_distance_mm) {
    InspectionResult result;
    result.transaction_id = response.transaction_id;
    result.station_id = station_id;
    result.escalator_id = escalator_id;
    result.task_id = task_id;
    result.timestamp = response.ts_received;
    result.working_distance_mm = working_distance_mm;

    auto extracted = extractor_.extract(response.results, min_confidence_);
    if (!extracted.task_id.empty()) {
        result.task_id = extracted.task_id;
    } else {
        result.task_id = "Unknown";
    }

    for (const auto& det : response.results) {
        result.ai_detections.push_back(det);
    }
    result.abnormal = extracted.abnormals;

    if (sub_task == InspectionSubTask::GapInspection) {
        // Task2 uses riser_cleats as up edge instead of tread_cleat_up
        const auto& up_cleats = (result.task_id == "T2" && extracted.up_cleats.empty())
                                ? extracted.riser_cleats : extracted.up_cleats;

        if (!up_cleats.empty() && !extracted.dn_cleats.empty()) {
            EdgeFitter fitter;
            Line2D up_line = fitter.fit_up_edge(up_cleats);
            Line2D dn_line = fitter.fit_dn_edge(extracted.dn_cleats);

            if (up_line.valid) {
                result.up_edge.valid = true;
                result.up_edge.slope = up_line.slope;
                result.up_edge.intercept = up_line.intercept;
            }
            if (dn_line.valid) {
                result.dn_edge.valid = true;
                result.dn_edge.slope = dn_line.slope;
                result.dn_edge.intercept = dn_line.intercept;
            }

            if (up_line.valid && dn_line.valid) {
                // Estimate image dimensions from detection coordinates
                double max_y = 720.0; // fallback
                for (const auto& det : response.results) {
                    for (const auto& c : det.coordinates) {
                        if (c.second > max_y) max_y = c.second;
                    }
                }
                const double margin = max_y * 0.10; // 10% top/bottom margin

                GapLocalizer localizer;
                auto gaps = localizer.locate_all_gaps(up_line, dn_line, extracted.gaps);

                for (const auto& gap : gaps) {
                    // Gap position filtering: discard if near image top/bottom
                    double gap_center_y = (gap.up_point.y + gap.dn_point.y) / 2.0;
                    if (gap_center_y < margin || gap_center_y > max_y - margin) {
                        Logger::debug("Postprocess: gap filtered (y=" +
                                      std::to_string(gap_center_y) + " near edge)");
                        continue;
                    }

                    if (left_data && right_data) {
                        double dist = gap_dist_.calculate(gap, left_data, left_size,
                                                         right_data, right_size);
                        GapLineInfo info;
                        info.valid = true;
                        info.up_x = gap.up_point.x;
                        info.up_y = gap.up_point.y;
                        info.dn_x = gap.dn_point.x;
                        info.dn_y = gap.dn_point.y;
                        info.gap_distance_mm = dist;
                        result.gap_lines.push_back(info);

                        if (dist > result.gap_distance_mm) {
                            result.gap_distance_mm = dist;
                        }
                    }
                }

                if (result.gap_distance_mm > 0) {
                    Logger::info("Postprocess: gap distance = "
                                 + std::to_string(result.gap_distance_mm) + "mm (task "
                                 + result.task_id + ")");
                }
            }
        }
    }

    if (record_mgr_ && !record_mgr_->active_record().empty()) {
        std::string subfolder = (sub_task == InspectionSubTask::Installation) ? "installation" : "inspection";

        json result_json;
        result_json["record_id"] = record_mgr_->active_record();
        result_json["pair_index"] = pair_index;
        result_json["timestamp"] = result.timestamp;
        result_json["task_id"] = result.task_id;
        result_json["station_id"] = result.station_id;
        result_json["escalator_id"] = result.escalator_id;
        result_json["working_distance_mm"] = result.working_distance_mm;
        result_json["gap_distance_mm"] = result.gap_distance_mm;

        json dets = json::array();
        for (const auto& d : result.ai_detections) {
            dets.push_back({
                {"label_id", d.label_id},
                {"confidence", d.confidence}
            });
        }
        result_json["ai_detections"] = dets;
        result_json["abnormal"] = json::array();
        for (const auto& a : result.abnormal) {
            result_json["abnormal"].push_back({{"label_id", a.label_id}, {"confidence", a.confidence}});
        }

        record_mgr_->save_stereo_result(record_mgr_->active_record(),
                                        subfolder, "", pair_index, result_json.dump());
    }

    return result;
}

InspectionResult PostprocessModule::process_ai_test(const DetectionResponse& response,
                                                     int pair_index) {
    InspectionResult result;
    result.transaction_id = response.transaction_id;
    result.timestamp = response.ts_received;

    for (const auto& det : response.results) {
        result.ai_detections.push_back(det);
    }

    if (record_mgr_ && !record_mgr_->active_record().empty()) {
        json result_json;
        result_json["pair_index"] = pair_index;
        result_json["timestamp"] = result.timestamp;

        json dets = json::array();
        for (const auto& d : response.results) {
            dets.push_back({
                {"label_id", d.label_id},
                {"confidence", d.confidence}
            });
        }
        result_json["detections"] = dets;

        record_mgr_->save_stereo_result(record_mgr_->active_record(),
                                        "", "", pair_index, result_json.dump());
    }

    return result;
}

} // namespace ecids_core
