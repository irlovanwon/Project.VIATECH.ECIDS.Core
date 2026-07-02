/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Locate up/down gap lines using 4 fitted edges
 * Date: 2026-06-18
 * Modification: 2026-07-02 Up gap lines (up_short→dn_long) + down gap lines (up_long→dn_short)
 */

#ifndef ECIDS_CORE_POSTPROCESS_GAPLOCALIZER_H
#define ECIDS_CORE_POSTPROCESS_GAPLOCALIZER_H

#pragma once

#include "ecids_core/common/Types.h"
#include <vector>

namespace ecids_core {

struct GapLine {
    Point2D up_point;
    Point2D dn_point;
    double length_px = 0.0;
    GapLineType type = GapLineType::Unknown;
    bool valid = false;
};

class GapLocalizer {
public:
    // Legacy API: single up/down edge pair
    GapLine locate_gap(const Line2D& up_edge, const Line2D& dn_edge,
                       const std::vector<Detection>& gaps);
    std::vector<GapLine> locate_all_gaps(const Line2D& up_edge, const Line2D& dn_edge,
                                         const std::vector<Detection>& gaps);

    // New 4-edge API:
    // Up gap lines: from bottom-mid of up cleat short, perpendicular to up_short until dn_long
    std::vector<GapLine> locate_up_gap_lines(
        const Line2D& up_short, const Line2D& dn_long,
        const std::vector<ClassifiedCleat>& up_short_cleats);

    // Down gap lines: from bottom-mid of up cleat long, perpendicular to up_long until dn_short
    std::vector<GapLine> locate_down_gap_lines(
        const Line2D& up_long, const Line2D& dn_short,
        const std::vector<ClassifiedCleat>& up_long_cleats);

private:
    Point2D perpendicular_to_line_(const Line2D& start_edge, const Line2D& end_edge,
                                    const Point2D& start);
};

} // namespace ecids_core

#endif // ECIDS_CORE_POSTPROCESS_GAPLOCALIZER_H
