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
#include <opencv2/opencv.hpp>

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
                                                     const uint8_t* left_data, size_t left_size,
                                                     const uint8_t* right_data, size_t right_size,
                                                     int pair_index) {
    InspectionResult result;
    result.transaction_id = response.transaction_id;
    result.task_id = "AI Test";
    result.timestamp = response.ts_received;

    auto extracted = extractor_.extract(response.results, min_confidence_);
    if (!extracted.task_id.empty()) {
        result.task_id = extracted.task_id;
    }
    for (const auto& det : response.results) {
        result.ai_detections.push_back(det);
    }
    result.abnormal = extracted.abnormals;

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
            double max_y = 720.0;
            for (const auto& det : response.results) {
                for (const auto& c : det.coordinates) {
                    if (c.second > max_y) max_y = c.second;
                }
            }
            const double margin = max_y * 0.10;

            GapLocalizer localizer;
            auto gaps = localizer.locate_all_gaps(up_line, dn_line, extracted.gaps);

            for (const auto& gap : gaps) {
                double gap_center_y = (gap.up_point.y + gap.dn_point.y) / 2.0;
                if (gap_center_y < margin || gap_center_y > max_y - margin) {
                    continue;
                }
                if (left_data && right_data && left_size > 0 && right_size > 0) {
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
        }
    }

    if (record_mgr_ && !record_mgr_->active_record().empty()) {
        json result_json;
        result_json["pair_index"] = pair_index;
        result_json["timestamp"] = result.timestamp;
        result_json["task_id"] = result.task_id;
        result_json["gap_distance_mm"] = result.gap_distance_mm;

        if (result.up_edge.valid) {
            result_json["up_edge"] = json::object({{"slope", result.up_edge.slope}, {"intercept", result.up_edge.intercept}});
        }
        if (result.dn_edge.valid) {
            result_json["dn_edge"] = json::object({{"slope", result.dn_edge.slope}, {"intercept", result.dn_edge.intercept}});
        }

        json gap_lines = json::array();
        for (const auto& gl : result.gap_lines) {
            gap_lines.push_back(json::object({
                {"up_x", gl.up_x}, {"up_y", gl.up_y},
                {"dn_x", gl.dn_x}, {"dn_y", gl.dn_y},
                {"gap_distance_mm", gl.gap_distance_mm}
            }));
        }
        result_json["gap_lines"] = gap_lines;

        json dets = json::array();
        for (const auto& d : result.ai_detections) {
            dets.push_back(json::object({{"label_id", d.label_id}, {"confidence", d.confidence}}));
        }
        result_json["ai_detections"] = dets;

        json abn = json::array();
        for (const auto& a : result.abnormal) {
            abn.push_back(json::object({{"label_id", a.label_id}, {"confidence", a.confidence}}));
        }
        result_json["abnormal"] = abn;

        record_mgr_->save_stereo_result(record_mgr_->active_record(),
                                        "", "", pair_index, result_json.dump());
    }

    if (record_mgr_ && !record_mgr_->active_record().empty()) {
        if (left_data && left_size > 0) {
            auto ann = annotate_image_(left_data, left_size, result);
            if (!ann.empty()) {
                record_mgr_->save_image(record_mgr_->active_record(), "",
                    "AL", pair_index, ann.data(), ann.size());
            }
        }
        // Right image: save raw only (no annotation)
        // AI detections are from the left image perspective — drawing them
        // on the right image produces incorrect overlays.
        if (right_data && right_size > 0) {
            record_mgr_->save_image(record_mgr_->active_record(), "",
                "R", pair_index, right_data, right_size);
        }
    }

    return result;
}

std::vector<uint8_t> PostprocessModule::annotate_image_(const uint8_t* data, size_t size,
                                                         const InspectionResult& result) {
    std::vector<uint8_t> output;
    if (!data || size == 0) return output;

    cv::Mat img = cv::imdecode(cv::Mat(1, (int)size, CV_8UC1,
        const_cast<uint8_t*>(data)), cv::IMREAD_COLOR);
    if (img.empty()) return output;

    int w = img.cols;
    if (result.up_edge.valid) {
        cv::Point p1(0, (int)(result.up_edge.intercept));
        cv::Point p2(w, (int)(result.up_edge.slope * w + result.up_edge.intercept));
        cv::line(img, p1, p2, cv::Scalar(0, 255, 0), 2);
    }
    if (result.dn_edge.valid) {
        cv::Point p1(0, (int)(result.dn_edge.intercept));
        cv::Point p2(w, (int)(result.dn_edge.slope * w + result.dn_edge.intercept));
        cv::line(img, p1, p2, cv::Scalar(0, 0, 255), 2);
    }

    for (const auto& gl : result.gap_lines) {
        cv::Point p1((int)gl.up_x, (int)gl.up_y);
        cv::Point p2((int)gl.dn_x, (int)gl.dn_y);
        cv::line(img, p1, p2, cv::Scalar(255, 0, 0), 2);
        if (gl.gap_distance_mm > 0) {
            std::string label = std::to_string(gl.gap_distance_mm).substr(0, 5) + "mm";
            cv::putText(img, label, p2 + cv::Point(5, -5),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 0, 0), 1);
        }
    }

    for (const auto& det : result.ai_detections) {
        if (det.coordinates.empty()) continue;
        double min_x = det.coordinates[0].first;
        double min_y = det.coordinates[0].second;
        double max_x = min_x, max_y = min_y;
        for (const auto& c : det.coordinates) {
            min_x = std::min(min_x, c.first);
            min_y = std::min(min_y, c.second);
            max_x = std::max(max_x, c.first);
            max_y = std::max(max_y, c.second);
        }
        cv::rectangle(img, cv::Point((int)min_x, (int)min_y),
            cv::Point((int)max_x, (int)max_y), cv::Scalar(255, 255, 0), 2);
        cv::putText(img, det.label_id, cv::Point((int)min_x, (int)min_y - 5),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 0), 1);
    }

    cv::imencode(".jpg", img, output, {cv::IMWRITE_JPEG_QUALITY, 95});
    return output;
}

} // namespace ecids_core
