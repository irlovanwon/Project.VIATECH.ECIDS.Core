/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Extract label IDs from AI results
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented detection categorization
 */

#ifndef ECIDS_CORE_POSTPROCESS_DETECTIONEXTRACTOR_H
#define ECIDS_CORE_POSTPROCESS_DETECTIONEXTRACTOR_H

#pragma once

#include "ecids_core/common/Types.h"
#include <vector>
#include <string>

namespace ecids_core {

struct ExtractedDetections {
    std::string task_id;
    double task_confidence = 0.0;
    std::vector<Detection> gaps;
    std::vector<Detection> up_cleats;
    std::vector<Detection> dn_cleats;
    std::vector<Detection> riser_cleats;
    std::vector<Detection> abnormals;
};

class DetectionExtractor {
public:
    ExtractedDetections extract(const std::vector<Detection>& detections,
                                double min_confidence = 0.5);

    std::string determine_task(const std::vector<Detection>& detections,
                               double min_confidence = 0.5);
};

} // namespace ecids_core

#endif // ECIDS_CORE_POSTPROCESS_DETECTIONEXTRACTOR_H
