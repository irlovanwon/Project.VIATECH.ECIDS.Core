/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Disparity calculation with global shift matching
 * Date: 2026-06-18
 * Modification: 2026-07-02 Added global shift algorithm with 10 sets + RANSAC fitting
 */

#ifndef ECIDS_CORE_POSTPROCESS_DISPARITYCALCULATOR_H
#define ECIDS_CORE_POSTPROCESS_DISPARITYCALCULATOR_H

#pragma once

#include "ecids_core/common/Types.h"
#include <vector>
#include <cstdint>

namespace ecids_core {

struct DisparitySet {
    double average_disparity = 0.0;
    int shift = 0;
    std::vector<double> disparities;
};

class DisparityCalculator {
public:
    void init(double baseline_mm, double focal_length_px);

    // Single-point disparity (legacy)
    double calculate_disparity(const uint8_t* left_data, size_t left_size,
                               const uint8_t* right_data, size_t right_size,
                               const Point2D& point, int window_radius = 15);

    // Global shift disparity matching:
    // Given sorted reference points from left and right images,
    // compute 10 sets of disparity using shifts ±1..±5
    std::vector<DisparitySet> global_shift_disparity(
        const std::vector<Point2D>& left_points,
        const std::vector<Point2D>& right_points,
        const uint8_t* left_data, size_t left_size,
        const uint8_t* right_data, size_t right_size,
        int window_radius = 15);

    // RANSAC fitting of disparity list to correct outliers
    std::vector<double> ransac_fit_disparity(const std::vector<double>& disparities,
                                              double inlier_threshold = 5.0);

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
