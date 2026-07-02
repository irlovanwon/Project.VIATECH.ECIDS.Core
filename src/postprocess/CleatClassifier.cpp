/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: CleatClassifier implementation — long/short classification
 * Date: 2026-07-02
 */

#include "ecids_core/postprocess/CleatClassifier.h"
#include "ecids_core/common/Logger.h"
#include <algorithm>
#include <cmath>

namespace ecids_core {

Point2D CleatClassifier::detection_bottom_mid(const Detection& det) {
    if (det.coordinates.empty()) return {};
    double max_y = det.coordinates[0].second;
    double sum_x = 0;
    int count = 0;
    for (const auto& c : det.coordinates) {
        if (std::abs(c.second - max_y) < 3.0) {
            sum_x += c.first;
            ++count;
        }
        if (c.second > max_y) {
            max_y = c.second;
            sum_x = c.first;
            count = 1;
        }
    }
    Point2D p;
    p.x = (count > 0) ? sum_x / count : det.coordinates[0].first;
    p.y = max_y;
    return p;
}

double CleatClassifier::detection_bottom_y(const Detection& det) {
    double max_y = 0;
    for (const auto& c : det.coordinates) {
        if (c.second > max_y) max_y = c.second;
    }
    return max_y;
}

double CleatClassifier::detection_top_y(const Detection& det) {
    double min_y = 1e18;
    for (const auto& c : det.coordinates) {
        if (c.second < min_y) min_y = c.second;
    }
    return (min_y < 1e18) ? min_y : 0;
}

double CleatClassifier::detection_center_x(const Detection& det) {
    if (det.coordinates.empty()) return 0;
    double sum = 0;
    for (const auto& c : det.coordinates) sum += c.first;
    return sum / det.coordinates.size();
}

ClassifiedCleat CleatClassifier::make_classified_(const Detection& det, CleatType type) {
    ClassifiedCleat cc;
    cc.detection = det;
    cc.type = type;
    cc.bottom_mid = detection_bottom_mid(det);
    cc.bottom_y = detection_bottom_y(det);
    cc.top_y = detection_top_y(det);
    cc.center_x = detection_center_x(det);
    return cc;
}

void CleatClassifier::split_by_median_(
    const std::vector<Detection>& cleats,
    bool use_bottom,
    std::vector<ClassifiedCleat>& long_out,
    std::vector<ClassifiedCleat>& short_out) {

    if (cleats.empty()) return;

    std::vector<double> values;
    for (const auto& c : cleats) {
        values.push_back(use_bottom ? detection_bottom_y(c) : detection_top_y(c));
    }
    std::sort(values.begin(), values.end());
    double median = values[values.size() / 2];

    for (const auto& det : cleats) {
        double val = use_bottom ? detection_bottom_y(det) : detection_top_y(det);
        if (use_bottom) {
            if (val >= median) {
                long_out.push_back(make_classified_(det, CleatType::Long));
            } else {
                short_out.push_back(make_classified_(det, CleatType::Short));
            }
        } else {
            if (val <= median) {
                long_out.push_back(make_classified_(det, CleatType::Long));
            } else {
                short_out.push_back(make_classified_(det, CleatType::Short));
            }
        }
    }
}

CleatClassificationResult CleatClassifier::classify(
    const std::vector<Detection>& up_cleats,
    const std::vector<Detection>& dn_cleats,
    const std::vector<Detection>& riser_cleats,
    const std::vector<Detection>& gaps,
    const std::string& task_id) {

    CleatClassificationResult result;

    bool is_task2 = (task_id == "T2");

    // Up cleats: for task1 use tread_cleat_up, for task2 use riser_cleat
    const auto& up = is_task2 ? riser_cleats : up_cleats;

    if (is_task2) {
        // Task2: all up cleats are "long", no short
        for (const auto& det : up) {
            result.up_long.push_back(make_classified_(det, CleatType::Long));
        }
    } else {
        // Task1: split up cleats by bottom_y
        // Long = bottom edge closer to gap (larger bottom_y)
        split_by_median_(up, true, result.up_long, result.up_short);
    }

    // Down cleats: split by top_y for both tasks
    // Long = top edge closer to gap (smaller top_y)
    if (is_task2) {
        // Task2: all down cleats are short per spec (no down long for task2)
        for (const auto& det : dn_cleats) {
            result.dn_short.push_back(make_classified_(det, CleatType::Short));
        }
    } else {
        split_by_median_(dn_cleats, false, result.dn_long, result.dn_short);
    }

    Logger::debug("CleatClassifier: up_long=" + std::to_string(result.up_long.size())
                  + " up_short=" + std::to_string(result.up_short.size())
                  + " dn_long=" + std::to_string(result.dn_long.size())
                  + " dn_short=" + std::to_string(result.dn_short.size())
                  + " task=" + task_id);

    return result;
}

} // namespace ecids_core
