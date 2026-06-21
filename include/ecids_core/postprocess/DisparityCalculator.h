/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Locally weighted disparity calculation
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented locally weighted disparity
 */

#ifndef ECIDS_CORE_POSTPROCESS_DISPARITYCALCULATOR_H
#define ECIDS_CORE_POSTPROCESS_DISPARITYCALCULATOR_H

#pragma once

#include "ecids_core/postprocess/EdgeFitter.h"
#include "ecids_core/common/Types.h"
#include <vector>
#include <cstdint>

namespace ecids_core {

class DisparityCalculator {
public:
    void init(double baseline_mm, double focal_length_px);

    double calculate_disparity(const uint8_t* left_data, size_t left_size,
                               const uint8_t* right_data, size_t right_size,
                               const Point2D& point, int window_radius = 15);

    double calculate_distance(double disparity_px) const;

    double baseline_mm() const { return baseline_mm_; }
    double focal_length_px() const { return focal_length_px_; }

private:
    double baseline_mm_ = 120.0;
    double focal_length_px_ = 0.0;

    double template_match_(const uint8_t* left, int lw, int lh,
                           const uint8_t* right, int rw, int rh,
                           int px, int py, int window_radius);
};

} // namespace ecids_core

#endif // ECIDS_CORE_POSTPROCESS_DISPARITYCALCULATOR_H
