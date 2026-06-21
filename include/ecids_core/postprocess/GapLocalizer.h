/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Locate gap features in stereo images
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented gap line localization
 */

#ifndef ECIDS_CORE_POSTPROCESS_GAPLOCALIZER_H
#define ECIDS_CORE_POSTPROCESS_GAPLOCALIZER_H

#pragma once

#include "ecids_core/postprocess/EdgeFitter.h"
#include "ecids_core/common/Types.h"
#include <vector>

namespace ecids_core {

struct GapLine {
    Point2D up_point;
    Point2D dn_point;
    double length_px = 0.0;
    bool valid = false;
};

class GapLocalizer {
public:
    GapLine locate_gap(const Line2D& up_edge, const Line2D& dn_edge,
                       const std::vector<Detection>& gaps);

    std::vector<GapLine> locate_all_gaps(const Line2D& up_edge, const Line2D& dn_edge,
                                         const std::vector<Detection>& gaps);

private:
    Point2D perpendicular_to_dn_(const Line2D& up, const Line2D& dn,
                                  const Point2D& start);
};

} // namespace ecids_core

#endif // ECIDS_CORE_POSTPROCESS_GAPLOCALIZER_H
