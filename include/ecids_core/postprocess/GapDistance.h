/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Calculate real gap distance from disparity with depth reference + offset
 * Date: 2026-06-18
 * Modification: 2026-07-02 Added depth reference selection and offset correction
 */

#ifndef ECIDS_CORE_POSTPROCESS_GAPDISTANCE_H
#define ECIDS_CORE_POSTPROCESS_GAPDISTANCE_H

#pragma once

#include "ecids_core/postprocess/DisparityCalculator.h"
#include "ecids_core/postprocess/GapLocalizer.h"

namespace ecids_core {

class GapDistance {
public:
    void init(double baseline_mm, double focal_length_px);
    void set_depth_reference(double ref_mm) { depth_reference_mm_ = ref_mm; }
    void set_offset(double offset_mm) { offset_mm_ = offset_mm; }

    double calculate(const GapLine& gap_line,
                     const uint8_t* left_data, size_t left_size,
                     const uint8_t* right_data, size_t right_size);

    double from_disparity(double disparity_px) const;

    // Select best disparity set from candidates (closest to depth reference)
    double select_best_disparity(const std::vector<double>& disparity_candidates) const;

private:
    DisparityCalculator disp_calc_;
    double depth_reference_mm_ = 0.0;
    double offset_mm_ = 0.0;
};

} // namespace ecids_core

#endif // ECIDS_CORE_POSTPROCESS_GAPDISTANCE_H
