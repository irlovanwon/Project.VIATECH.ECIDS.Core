/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: PostprocessModule — full algorithm pipeline with annotated images
 * Date: 2026-06-18
 * Modification: 2026-07-02 Integrated cleat classification, 4-edge hybrid RANSAC,
 *               up/down gap lines, same gap checking, depth reference, annotated images,
 *               SPSC DB writer integration
 */

#include "ecids_core/postprocess/PostprocessModule.h"
#include "ecids_core/postprocess/EdgeFitter.h"
#include "ecids_core/postprocess/GapLocalizer.h"
#include "ecids_core/database/RecordManager.h"
#include "ecids_core/database/DatabaseWriter.h"
#include "ecids_core/common/Logger.h"

#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>
#include <algorithm>
#include <cmath>

namespace ecids_core {

using json = nlohmann::json;

static cv::Mat decode_image_(const uint8_t* data, size_t size) {
    if (!data || size == 0) return {};

    // Try JPG decode
    cv::Mat img = cv::imdecode(cv::Mat(1, static_cast<int>(size), CV_8UC1,
        const_cast<uint8_t*>(data)), cv::IMREAD_COLOR);
    if (!img.empty()) return img;

    // Try raw BGRA
    if (size % 4 == 0) {
        int w = 1920;
        int h = static_cast<int>(size / (static_cast<size_t>(w) * 4));
        if (h > 0 && static_cast<size_t>(w) * h * 4 == size) {
            cv::Mat bgra(h, w, CV_8UC4, const_cast<uint8_t*>(data));
            cv::Mat bgr;
            cv::cvtColor(bgra, bgr, cv::COLOR_BGRA2BGR);
            return bgr;
        }
    }
    return {};
}

void PostprocessModule::init(double min_confidence, double baseline_mm, double focal_length_px) {
    min_confidence_ = min_confidence;
    config_.min_confidence = min_confidence;
    gap_dist_.init(baseline_mm, focal_length_px);
}

void PostprocessModule::run_gap_pipeline_(InspectionResult& result,
                                           const ExtractedDetections& extracted,
                                           const uint8_t* left_data, size_t left_size,
                                           const uint8_t* right_data, size_t right_size,
                                           int image_height) {

    // Use task-appropriate up cleats
    const auto& up_cleats = (result.task_id == "T2" && extracted.up_cleats.empty())
                            ? extracted.riser_cleats : extracted.up_cleats;

    if (up_cleats.empty() || extracted.dn_cleats.empty()) return;

    // Classify cleats into long/short
    auto classified = cleat_classifier_.classify(
        extracted.up_cleats, extracted.dn_cleats,
        extracted.riser_cleats, extracted.gaps, result.task_id);

    // Region localization
    result.region = region_localizer_.localize(
        extracted.gaps, extracted.up_cleats, extracted.dn_cleats,
        extracted.riser_cleats, image_height);

    // 4-edge fitting via hybrid RANSAC
    double inlier_thresh = config_.edge_inlier_threshold;

    if (!classified.up_long.empty()) {
        auto pts = edge_fitter_.extract_classified_bottom_points(classified.up_long);
        result.edges.up_long.valid = true;
        auto line = edge_fitter_.hybrid_ransac_fit(pts, inlier_thresh);
        result.edges.up_long.slope = line.slope;
        result.edges.up_long.intercept = line.intercept;
        result.edges.up_long.valid = line.valid;
    }
    if (!classified.up_short.empty()) {
        auto pts = edge_fitter_.extract_classified_bottom_points(classified.up_short);
        auto line = edge_fitter_.hybrid_ransac_fit(pts, inlier_thresh);
        result.edges.up_short.slope = line.slope;
        result.edges.up_short.intercept = line.intercept;
        result.edges.up_short.valid = line.valid;
    }
    if (!classified.dn_long.empty()) {
        auto pts = edge_fitter_.extract_classified_top_points(classified.dn_long);
        auto line = edge_fitter_.hybrid_ransac_fit(pts, inlier_thresh);
        result.edges.dn_long.slope = line.slope;
        result.edges.dn_long.intercept = line.intercept;
        result.edges.dn_long.valid = line.valid;
    }
    if (!classified.dn_short.empty()) {
        auto pts = edge_fitter_.extract_classified_top_points(classified.dn_short);
        auto line = edge_fitter_.hybrid_ransac_fit(pts, inlier_thresh);
        result.edges.dn_short.slope = line.slope;
        result.edges.dn_short.intercept = line.intercept;
        result.edges.dn_short.valid = line.valid;
    }

    // Backward-compat: set up_edge/dn_edge from long edges
    if (result.edges.up_long.valid) {
        // FittedEdge and FittedEdge are same struct
    }

    // Gap line localization
    GapLocalizer localizer;

    // Up gap lines: between up_short and dn_long
    if (result.edges.up_short.valid && result.edges.dn_long.valid &&
        !classified.up_short.empty()) {

        Line2D up_short_l{result.edges.up_short.slope, result.edges.up_short.intercept, true};
        Line2D dn_long_l{result.edges.dn_long.slope, result.edges.dn_long.intercept, true};

        auto up_gaps = localizer.locate_up_gap_lines(up_short_l, dn_long_l,
                                                      classified.up_short);
        for (const auto& gl : up_gaps) {
            if (!gl.valid) continue;

            // Gap filter: discard if near image top/bottom
            double gap_center_y = (gl.up_point.y + gl.dn_point.y) / 2.0;
            double margin = image_height * config_.gap_filter_margin;
            if (gap_center_y < margin || gap_center_y > image_height - margin) {
                continue;
            }

            GapLineInfo info;
            info.valid = true;
            info.type = GapLineType::UpGap;
            info.up_x = gl.up_point.x;
            info.up_y = gl.up_point.y;
            info.dn_x = gl.dn_point.x;
            info.dn_y = gl.dn_point.y;

            if (left_data && right_data) {
                double dist = gap_dist_.calculate(gl,
                    left_data, left_size, right_data, right_size);
                info.gap_distance_mm = dist;
                
                if (dist > result.gap_distance_mm) {
                    result.gap_distance_mm = dist;
                }
            }
            result.gap_lines.push_back(info);
        }
    }

    // Down gap lines: between up_long and dn_short
    if (result.edges.up_long.valid && result.edges.dn_short.valid &&
        !classified.up_long.empty()) {

        Line2D up_long_l{result.edges.up_long.slope, result.edges.up_long.intercept, true};
        Line2D dn_short_l{result.edges.dn_short.slope, result.edges.dn_short.intercept, true};

        auto dn_gaps = localizer.locate_down_gap_lines(up_long_l, dn_short_l,
                                                        classified.up_long);
        for (const auto& gl : dn_gaps) {
            if (!gl.valid) continue;

            double gap_center_y = (gl.up_point.y + gl.dn_point.y) / 2.0;
            double margin = image_height * config_.gap_filter_margin;
            if (gap_center_y < margin || gap_center_y > image_height - margin) {
                continue;
            }

            GapLineInfo info;
            info.valid = true;
            info.type = GapLineType::DownGap;
            info.up_x = gl.up_point.x;
            info.up_y = gl.up_point.y;
            info.dn_x = gl.dn_point.x;
            info.dn_y = gl.dn_point.y;

            if (left_data && right_data) {
                double dist = gap_dist_.calculate(gl,
                    left_data, left_size, right_data, right_size);
                info.gap_distance_mm = dist;
                if (dist > result.gap_distance_mm) {
                    result.gap_distance_mm = dist;
                }
            }
            result.gap_lines.push_back(info);
        }
    }

    // Fallback: if no 4-edge gap lines, try simple 2-edge
    if (result.gap_lines.empty() && result.edges.up_long.valid && result.edges.dn_short.valid) {
        Line2D up_l{result.edges.up_long.slope, result.edges.up_long.intercept, true};
        Line2D dn_l{result.edges.dn_short.slope, result.edges.dn_short.intercept, true};

        for (const auto& gap : extracted.gaps) {
            std::vector<Detection> single = {gap};
            auto gl = localizer.locate_gap(up_l, dn_l, single);
            if (!gl.valid) continue;

            double gap_center_y = (gl.up_point.y + gl.dn_point.y) / 2.0;
            double margin = image_height * config_.gap_filter_margin;
            if (gap_center_y < margin || gap_center_y > image_height - margin) continue;

            GapLineInfo info;
            info.valid = true;
            info.type = GapLineType::DownGap;
            info.up_x = gl.up_point.x;
            info.up_y = gl.up_point.y;
            info.dn_x = gl.dn_point.x;
            info.dn_y = gl.dn_point.y;

            if (left_data && right_data) {
                double dist = gap_dist_.calculate(gl,
                    left_data, left_size, right_data, right_size);
                info.gap_distance_mm = dist;
                if (dist > result.gap_distance_mm) {
                    result.gap_distance_mm = dist;
                }
            }
            result.gap_lines.push_back(info);
        }
    }
}

std::vector<uint8_t> PostprocessModule::annotate_image_(const uint8_t* data, size_t size,
                                                         const InspectionResult& result,
                                                         bool stereo_annotations_only) {
    std::vector<uint8_t> output;
    cv::Mat img = decode_image_(data, size);
    if (img.empty()) return output;

    int w = img.cols;

    // Draw 4 edges
    auto draw_edge = [&](const FittedEdge& e, const cv::Scalar& color) {
        if (!e.valid) return;
        cv::line(img, cv::Point(0, static_cast<int>(e.intercept)),
                 cv::Point(w, static_cast<int>(e.slope * w + e.intercept)),
                 color, 2);
    };

    draw_edge(result.edges.up_long, cv::Scalar(0, 255, 0));
    draw_edge(result.edges.up_short, cv::Scalar(0, 200, 100));
    draw_edge(result.edges.dn_long, cv::Scalar(0, 0, 255));
    draw_edge(result.edges.dn_short, cv::Scalar(100, 0, 200));

    // Draw gap lines
    for (const auto& gl : result.gap_lines) {
        cv::Point p1(static_cast<int>(gl.up_x), static_cast<int>(gl.up_y));
        cv::Point p2(static_cast<int>(gl.dn_x), static_cast<int>(gl.dn_y));
        cv::Scalar color = (gl.type == GapLineType::UpGap)
            ? cv::Scalar(255, 0, 0) : cv::Scalar(0, 165, 255);
        cv::line(img, p1, p2, color, 2);

        if (gl.gap_distance_mm > 0) {
            std::string label = std::to_string(gl.gap_distance_mm).substr(0, 5) + "mm";
            cv::putText(img, label, p2 + cv::Point(5, -5),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);
        }
    }

    // Draw detection boxes (unless stereo-only mode)
    if (!stereo_annotations_only) {
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
            cv::rectangle(img, cv::Point(static_cast<int>(min_x), static_cast<int>(min_y)),
                cv::Point(static_cast<int>(max_x), static_cast<int>(max_y)),
                cv::Scalar(255, 255, 0), 2);
            cv::putText(img, det.label_id, cv::Point(static_cast<int>(min_x), static_cast<int>(min_y) - 5),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 0), 1);
        }
    }

    cv::imencode(".jpg", img, output, {cv::IMWRITE_JPEG_QUALITY, 95});
    return output;
}

std::vector<uint8_t> PostprocessModule::annotate_image_ai_only_(const uint8_t* data, size_t size,
                                                                  const InspectionResult& result) {
    std::vector<uint8_t> output;
    cv::Mat img = decode_image_(data, size);
    if (img.empty()) return output;

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
        cv::rectangle(img, cv::Point(static_cast<int>(min_x), static_cast<int>(min_y)),
            cv::Point(static_cast<int>(max_x), static_cast<int>(max_y)),
            cv::Scalar(255, 255, 0), 2);
        cv::putText(img, det.label_id, cv::Point(static_cast<int>(min_x), static_cast<int>(min_y) - 5),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 0), 1);
    }

    cv::imencode(".jpg", img, output, {cv::IMWRITE_JPEG_QUALITY, 95});
    return output;
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

    // Estimate image height
    int image_height = 1200;
    if (left_data && left_size > 0) {
        cv::Mat img = decode_image_(left_data, left_size);
        if (!img.empty()) image_height = img.rows;
    }

    // Same gap checking (left image only)
    if (sub_task == InspectionSubTask::GapInspection && !extracted.gaps.empty()) {
        if (same_gap_checker_.is_same_gap(extracted.gaps[0])) {
            Logger::debug("Postprocess: same gap detected — skipping");
            return result;
        }
    }

    if (sub_task == InspectionSubTask::GapInspection) {
        run_gap_pipeline_(result, extracted,
                          left_data, left_size,
                          right_data, right_size,
                          image_height);
    }

    // Generate annotated images
    if (left_data && left_size > 0) {
        result.annotated_left = annotate_image_(left_data, left_size, result);
    }
    if (right_data && right_size > 0) {
        result.annotated_right = annotate_image_(right_data, right_size, result);
    }

    // Save to database
    std::string subfolder = (sub_task == InspectionSubTask::Installation) ? "installation" : "inspection";
    if (record_mgr_ && !record_mgr_->active_record().empty()) {
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
            dets.push_back({{"label_id", d.label_id}, {"confidence", d.confidence}});
        }
        result_json["ai_detections"] = dets;
        result_json["abnormal"] = json::array();
        for (const auto& a : result.abnormal) {
            result_json["abnormal"].push_back({{"label_id", a.label_id}, {"confidence", a.confidence}});
        }

        if (result.edges.up_long.valid)
            result_json["up_edge_long"] = {{"slope", result.edges.up_long.slope}, {"intercept", result.edges.up_long.intercept}};
        if (result.edges.up_short.valid)
            result_json["up_edge_short"] = {{"slope", result.edges.up_short.slope}, {"intercept", result.edges.up_short.intercept}};
        if (result.edges.dn_long.valid)
            result_json["dn_edge_long"] = {{"slope", result.edges.dn_long.slope}, {"intercept", result.edges.dn_long.intercept}};
        if (result.edges.dn_short.valid)
            result_json["dn_edge_short"] = {{"slope", result.edges.dn_short.slope}, {"intercept", result.edges.dn_short.intercept}};

        if (!result.gap_lines.empty()) {
            json gls = json::array();
            for (const auto& gl : result.gap_lines) {
                gls.push_back({
                    {"type", (gl.type == GapLineType::UpGap) ? "up" : "down"},
                    {"up_x", gl.up_x}, {"up_y", gl.up_y},
                    {"dn_x", gl.dn_x}, {"dn_y", gl.dn_y},
                    {"gap_distance_mm", gl.gap_distance_mm}
                });
            }
            result_json["gap_lines"] = gls;
        }

        // Save via SPSC DatabaseWriter if available, else direct
        if (db_writer_) {
            db_writer_->enqueue_stereo_result(record_mgr_->active_record(),
                subfolder, "", pair_index, result_json.dump());
            db_writer_->enqueue_ai_result(record_mgr_->active_record(),
                subfolder, "L", pair_index, response.transaction_id);
            db_writer_->enqueue_ai_result(record_mgr_->active_record(),
                subfolder, "R", pair_index, response.transaction_id);

            // Save annotated images
            if (!result.annotated_left.empty()) {
                auto ptr = std::make_shared<std::vector<uint8_t>>(result.annotated_left);
                db_writer_->enqueue_image(record_mgr_->active_record(), subfolder,
                    "AL", pair_index, ptr);
            }
            if (!result.annotated_right.empty()) {
                auto ptr = std::make_shared<std::vector<uint8_t>>(result.annotated_right);
                db_writer_->enqueue_image(record_mgr_->active_record(), subfolder,
                    "AR", pair_index, ptr);
            }
        } else {
            record_mgr_->save_stereo_result(record_mgr_->active_record(),
                subfolder, "", pair_index, result_json.dump());
        }
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

    int image_height = 1200;
    if (left_data && left_size > 0) {
        cv::Mat img = decode_image_(left_data, left_size);
        if (!img.empty()) image_height = img.rows;
    }

    run_gap_pipeline_(result, extracted,
                      left_data, left_size,
                      right_data, right_size,
                      image_height);

    // Set 1: annotated with stereo results + AI detections
    if (left_data && left_size > 0) {
        result.annotated_left = annotate_image_(left_data, left_size, result, false);
        // Set 2: annotated with raw AI results only
        result.ai_annotated_left = annotate_image_ai_only_(left_data, left_size, result);
    }
    if (right_data && right_size > 0) {
        result.annotated_right = annotate_image_(right_data, right_size, result, false);
        result.ai_annotated_right = annotate_image_ai_only_(right_data, right_size, result);
    }

    // Save to database
    if (record_mgr_ && !record_mgr_->active_record().empty()) {
        json result_json;
        result_json["pair_index"] = pair_index;
        result_json["timestamp"] = result.timestamp;
        result_json["task_id"] = result.task_id;
        result_json["gap_distance_mm"] = result.gap_distance_mm;

        if (result.edges.up_long.valid)
            result_json["up_edge_long"] = {{"slope", result.edges.up_long.slope}, {"intercept", result.edges.up_long.intercept}};
        if (result.edges.up_short.valid)
            result_json["up_edge_short"] = {{"slope", result.edges.up_short.slope}, {"intercept", result.edges.up_short.intercept}};
        if (result.edges.dn_long.valid)
            result_json["dn_edge_long"] = {{"slope", result.edges.dn_long.slope}, {"intercept", result.edges.dn_long.intercept}};
        if (result.edges.dn_short.valid)
            result_json["dn_edge_short"] = {{"slope", result.edges.dn_short.slope}, {"intercept", result.edges.dn_short.intercept}};

        json gls = json::array();
        for (const auto& gl : result.gap_lines) {
            gls.push_back({
                {"type", (gl.type == GapLineType::UpGap) ? "up" : "down"},
                {"up_x", gl.up_x}, {"up_y", gl.up_y},
                {"dn_x", gl.dn_x}, {"dn_y", gl.dn_y},
                {"gap_distance_mm", gl.gap_distance_mm}
            });
        }
        result_json["gap_lines"] = gls;

        json dets = json::array();
        for (const auto& d : result.ai_detections) {
            dets.push_back({{"label_id", d.label_id}, {"confidence", d.confidence}});
        }
        result_json["ai_detections"] = dets;

        if (db_writer_) {
            db_writer_->enqueue_stereo_result(record_mgr_->active_record(),
                "", "", pair_index, result_json.dump());

            if (!result.annotated_left.empty()) {
                auto ptr = std::make_shared<std::vector<uint8_t>>(result.annotated_left);
                db_writer_->enqueue_image(record_mgr_->active_record(), "",
                    "AL", pair_index, ptr);
            }
            if (!result.annotated_right.empty()) {
                auto ptr = std::make_shared<std::vector<uint8_t>>(result.annotated_right);
                db_writer_->enqueue_image(record_mgr_->active_record(), "",
                    "AR", pair_index, ptr);
            }
            if (!result.ai_annotated_left.empty()) {
                auto ptr = std::make_shared<std::vector<uint8_t>>(result.ai_annotated_left);
                db_writer_->enqueue_image(record_mgr_->active_record(), "",
                    "AL2", pair_index, ptr);
            }
            if (!result.ai_annotated_right.empty()) {
                auto ptr = std::make_shared<std::vector<uint8_t>>(result.ai_annotated_right);
                db_writer_->enqueue_image(record_mgr_->active_record(), "",
                    "AR2", pair_index, ptr);
            }
        } else {
            record_mgr_->save_stereo_result(record_mgr_->active_record(),
                "", "", pair_index, result_json.dump());
        }
    }

    return result;
}

} // namespace ecids_core
