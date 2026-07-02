/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: GapDistance — disparity to real distance with depth reference + offset
 * Date: 2026-06-18
 * Modification: 2026-07-02 Added depth reference selection and offset correction
 */

#include "ecids_core/postprocess/GapDistance.h"

namespace ecids_core {

void GapDistance::init(double baseline_mm, double focal_length_px) {
    disp_calc_.init(baseline_mm, focal_length_px);
}

double GapDistance::calculate(const GapLine& gap_line,
                              const uint8_t* left_data, size_t left_size,
                              const uint8_t* right_data, size_t right_size) {
    if (!gap_line.valid) return 0.0;

    Point2D mid;
    mid.x = (gap_line.up_point.x + gap_line.dn_point.x) / 2.0;
    mid.y = (gap_line.up_point.y + gap_line.dn_point.y) / 2.0;

    double disparity = disp_calc_.calculate_disparity(
        left_data, left_size, right_data, right_size, mid);

    return from_disparity(disparity);
}

double GapDistance::from_disparity(double disparity_px) const {
    double dist = disp_calc_.calculate_distance(disparity_px);
    if (dist > 0 && offset_mm_ != 0.0) {
        dist += offset_mm_;
    }
    return dist;
}

double GapDistance::select_best_disparity(
    const std::vector<double>& disparity_candidates) const {

    if (disparity_candidates.empty()) return 0.0;
    if (depth_reference_mm_ <= 0.0) return disparity_candidates[0];

    double best_disp = disparity_candidates[0];
    double best_diff = 1e18;

    for (double disp : disparity_candidates) {
        if (disp < 0.1) continue;
        double depth = disp_calc_.calculate_distance(disp);
        double diff = std::abs(depth - depth_reference_mm_);
        if (diff < best_diff) {
            best_diff = diff;
            best_disp = disp;
        }
    }

    return best_disp;
}

} // namespace ecids_core
