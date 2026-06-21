/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Up/down edge line fitting
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented least-squares edge fitting with error correction
 */

#ifndef ECIDS_CORE_POSTPROCESS_EDGEFITTER_H
#define ECIDS_CORE_POSTPROCESS_EDGEFITTER_H

#pragma once

#include "ecids_core/common/Types.h"
#include <vector>

namespace ecids_core {

struct Line2D {
    double slope = 0.0;
    double intercept = 0.0;
    bool valid = false;
};

struct Point2D {
    double x = 0.0;
    double y = 0.0;
};

class EdgeFitter {
public:
    Line2D fit_line(const std::vector<Point2D>& points);

    Line2D fit_up_edge(const std::vector<Detection>& up_cleats);
    Line2D fit_dn_edge(const std::vector<Detection>& dn_cleats);

    std::vector<Point2D> extract_edge_points(const Detection& det, bool bottom_edge);
    Point2D line_intersection_x(const Line2D& line, double x);

private:
    Point2D centroid_(const std::vector<Point2D>& pts);
    std::vector<Point2D> remove_outliers_(std::vector<Point2D> pts, const Line2D& line, double threshold = 2.0);
};

} // namespace ecids_core

#endif // ECIDS_CORE_POSTPROCESS_EDGEFITTER_H
