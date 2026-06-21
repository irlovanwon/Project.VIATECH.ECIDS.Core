/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Calculate real gap distance from disparity
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented
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

    double calculate(const GapLine& gap_line,
                     const uint8_t* left_data, size_t left_size,
                     const uint8_t* right_data, size_t right_size);

    double from_disparity(double disparity_px) const;

private:
    DisparityCalculator disp_calc_;
};

} // namespace ecids_core

#endif // ECIDS_CORE_POSTPROCESS_GAPDISTANCE_H
