/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Classify cleats as long or short based on distance to gap region
 * Date: 2026-07-02
 */

#ifndef ECIDS_CORE_POSTPROCESS_CLEATCLASSIFIER_H
#define ECIDS_CORE_POSTPROCESS_CLEATCLASSIFIER_H

#pragma once

#include "ecids_core/common/Types.h"
#include <vector>

namespace ecids_core {

struct CleatClassificationResult {
    std::vector<ClassifiedCleat> up_long;
    std::vector<ClassifiedCleat> up_short;
    std::vector<ClassifiedCleat> dn_long;
    std::vector<ClassifiedCleat> dn_short;
};

class CleatClassifier {
public:
    // Classify up cleats into long/short based on gap region position
    // Long = bottom edge closer to gap region (larger bottom_y for up cleats)
    // Short = bottom edge farther from gap region
    CleatClassificationResult classify(
        const std::vector<Detection>& up_cleats,
        const std::vector<Detection>& dn_cleats,
        const std::vector<Detection>& riser_cleats,
        const std::vector<Detection>& gaps,
        const std::string& task_id);

    // Compute bottom midpoint of a detection
    static Point2D detection_bottom_mid(const Detection& det);
    static double detection_bottom_y(const Detection& det);
    static double detection_top_y(const Detection& det);
    static double detection_center_x(const Detection& det);

private:
    // Split cleats by median bottom_y (up) or top_y (down)
    void split_by_median_(
        const std::vector<Detection>& cleats,
        bool use_bottom,
        std::vector<ClassifiedCleat>& long_out,
        std::vector<ClassifiedCleat>& short_out);

    ClassifiedCleat make_classified_(const Detection& det, CleatType type);
};

} // namespace ecids_core

#endif // ECIDS_CORE_POSTPROCESS_CLEATCLASSIFIER_H
