/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Separate image into 3 regions: up cleat, gap, down cleat
 * Date: 2026-07-02
 */

#ifndef ECIDS_CORE_POSTPROCESS_REGIONLOCALIZER_H
#define ECIDS_CORE_POSTPROCESS_REGIONLOCALIZER_H

#pragma once

#include "ecids_core/common/Types.h"
#include <vector>

namespace ecids_core {

class RegionLocalizer {
public:
    // Locate 3 regions based on gap and cleat detections
    RegionInfo localize(
        const std::vector<Detection>& gaps,
        const std::vector<Detection>& up_cleats,
        const std::vector<Detection>& dn_cleats,
        const std::vector<Detection>& riser_cleats,
        int image_height);

    // Simple version using only gap y-position
    RegionInfo localize_from_gap(double gap_center_y, double gap_height,
                                 int image_height, double margin_ratio = 0.15);
};

} // namespace ecids_core

#endif // ECIDS_CORE_POSTPROCESS_REGIONLOCALIZER_H
