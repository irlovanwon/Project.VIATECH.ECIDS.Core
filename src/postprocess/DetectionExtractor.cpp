/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: DetectionExtractor implementation
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented
 */

#include "ecids_core/postprocess/DetectionExtractor.h"
#include "ecids_core/common/Logger.h"

namespace ecids_core {

ExtractedDetections DetectionExtractor::extract(const std::vector<Detection>& detections,
                                                double min_confidence) {
    ExtractedDetections result;

    double best_t1_conf = 0.0;
    double best_t2_conf = 0.0;

    for (const auto& det : detections) {
        if (det.confidence < min_confidence) continue;

        const std::string& label = det.label_id;

        if (label == "T1") {
            if (det.confidence > best_t1_conf) {
                best_t1_conf = det.confidence;
            }
        } else if (label == "T2") {
            if (det.confidence > best_t2_conf) {
                best_t2_conf = det.confidence;
            }
        } else if (label == "gap_t1" || label == "gap_t2") {
            result.gaps.push_back(det);
        } else if (label == "tread_cleat_up") {
            result.up_cleats.push_back(det);
        } else if (label == "tread_cleat_dn") {
            result.dn_cleats.push_back(det);
        } else if (label == "riser_cleat") {
            result.riser_cleats.push_back(det);
        } else if (label == "dust" || label == "deformation") {
            result.abnormals.push_back(det);
        }
    }

    if (best_t1_conf > 0 || best_t2_conf > 0) {
        if (best_t1_conf >= best_t2_conf) {
            result.task_id = "T1";
            result.task_confidence = best_t1_conf;
        } else {
            result.task_id = "T2";
            result.task_confidence = best_t2_conf;
        }
    }

    return result;
}

std::string DetectionExtractor::determine_task(const std::vector<Detection>& detections,
                                               double min_confidence) {
    double best_t1 = 0.0, best_t2 = 0.0;
    for (const auto& det : detections) {
        if (det.confidence < min_confidence) continue;
        if (det.label_id == "T1" && det.confidence > best_t1) best_t1 = det.confidence;
        if (det.label_id == "T2" && det.confidence > best_t2) best_t2 = det.confidence;
    }
    if (best_t1 == 0 && best_t2 == 0) return "";
    return (best_t1 >= best_t2) ? "T1" : "T2";
}

} // namespace ecids_core
