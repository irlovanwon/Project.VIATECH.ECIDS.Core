/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: GapDistance implementation
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented
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
    return disp_calc_.calculate_distance(disparity_px);
}

} // namespace ecids_core
