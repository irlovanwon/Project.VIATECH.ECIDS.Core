/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: PostprocessModule implementation
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented
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
                                            const uint8_t* right_data, size_t right_size) {
    InspectionResult result;
    result.transaction_id = response.transaction_id;
    result.station_id = station_id;
    result.escalator_id = escalator_id;
    result.task_id = task_id;
    result.timestamp = response.ts_received;

    auto extracted = extractor_.extract(response.results, min_confidence_);
    if (!extracted.task_id.empty()) {
        result.task_id = extracted.task_id;
    }

    for (const auto& det : response.results) {
        result.ai_detections.push_back(det);
    }
    result.abnormal = extracted.abnormals;

    if (!extracted.up_cleats.empty() && !extracted.dn_cleats.empty()) {
        EdgeFitter fitter;
        Line2D up_edge = fitter.fit_up_edge(extracted.up_cleats);
        Line2D dn_edge = fitter.fit_dn_edge(extracted.dn_cleats);

        if (up_edge.valid && dn_edge.valid) {
            GapLocalizer localizer;
            auto gaps = localizer.locate_all_gaps(up_edge, dn_edge, extracted.gaps);

            if (!gaps.empty() && left_data && right_data) {
                double best_distance = 0.0;
                for (const auto& gap : gaps) {
                    double dist = gap_dist_.calculate(gap, left_data, left_size,
                                                     right_data, right_size);
                    if (dist > best_distance) best_distance = dist;
                }
                result.gap_distance_mm = best_distance;

                Logger::info("Postprocess: gap distance = "
                             + std::to_string(best_distance) + "mm (task " + result.task_id + ")");
            }
        }
    }

    if (record_mgr_ && !record_mgr_->active_record().empty()) {
        json result_json;
        result_json["record_id"] = record_mgr_->active_record();
        result_json["timestamp"] = result.timestamp;
        result_json["task_id"] = result.task_id;
        result_json["station_id"] = result.station_id;
        result_json["escalator_id"] = result.escalator_id;
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

        record_mgr_->save_processed_result(record_mgr_->active_record(),
                                           result.timestamp, result_json.dump());

        json ai_json;
        ai_json["TransactionID"] = response.transaction_id;
        ai_json["DealerID"] = response.dealer_id;
        ai_json["TimestampReceived"] = response.ts_received;
        ai_json["TimestampReplied"] = response.ts_replied;
        json ai_dets = json::array();
        for (const auto& d : response.results) {
            ai_dets.push_back({
                {"LabelID", d.label_id},
                {"Confidence", std::to_string(d.confidence)},
                {"FileName", d.file_name}
            });
        }
        ai_json["Result"] = ai_dets;
        record_mgr_->save_ai_result(record_mgr_->active_record(),
                                    result.timestamp, ai_json.dump());
    }

    return result;
}

InspectionResult PostprocessModule::process_ai_test(const DetectionResponse& response) {
    InspectionResult result;
    result.transaction_id = response.transaction_id;
    result.timestamp = response.ts_received;

    for (const auto& det : response.results) {
        result.ai_detections.push_back(det);
    }

    if (record_mgr_ && !record_mgr_->active_record().empty()) {
        json result_json;
        result_json["timestamp"] = result.timestamp;

        json dets = json::array();
        for (const auto& d : response.results) {
            dets.push_back({
                {"label_id", d.label_id},
                {"confidence", d.confidence}
            });
        }
        result_json["detections"] = dets;

        record_mgr_->save_processed_result(record_mgr_->active_record(),
                                           result.timestamp, result_json.dump());
    }

    return result;
}

} // namespace ecids_core
