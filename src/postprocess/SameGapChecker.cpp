/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: SameGapChecker implementation — detect repeated gap
 * Date: 2026-07-02
 */

#include "ecids_core/postprocess/SameGapChecker.h"
#include <cmath>
#include <algorithm>

namespace ecids_core {

bool SameGapChecker::is_same_gap(const Detection& current_gap) {
    if (!config_.enabled) return false;
    if (current_gap.coordinates.empty()) return false;

    double min_x = 1e18, max_x = 0, min_y = 1e18, max_y = 0;
    for (const auto& c : current_gap.coordinates) {
        if (c.first < min_x) min_x = c.first;
        if (c.first > max_x) max_x = c.first;
        if (c.second < min_y) min_y = c.second;
        if (c.second > max_y) max_y = c.second;
    }
    double cx = (min_x + max_x) / 2.0;
    double cy = (min_y + max_y) / 2.0;
    double w = max_x - min_x;
    double h = max_y - min_y;

    if (!has_previous_) {
        has_previous_ = true;
        prev_center_x_ = cx;
        prev_center_y_ = cy;
        prev_width_ = w;
        prev_height_ = h;
        return false;
    }

    double dx = cx - prev_center_x_;
    double dy = cy - prev_center_y_;
    double dist = std::sqrt(dx * dx + dy * dy);

    double size_diff = 0;
    double prev_area = prev_width_ * prev_height_;
    double curr_area = w * h;
    if (prev_area > 0) {
        size_diff = std::abs(curr_area - prev_area) / prev_area;
    }

    bool same = (dist < config_.location_threshold &&
                 size_diff < config_.size_threshold);

    if (!same) {
        prev_center_x_ = cx;
        prev_center_y_ = cy;
        prev_width_ = w;
        prev_height_ = h;
    }

    return same;
}

} // namespace ecids_core
