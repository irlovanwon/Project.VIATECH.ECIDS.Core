/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Up/down edge line fitting with hybrid RANSAC
 * Date: 2026-06-18
 * Modification: 2026-07-02 4-edge fitting (up_long/up_short/dn_long/dn_short), hybrid RANSAC
 */

#ifndef ECIDS_CORE_POSTPROCESS_EDGEFITTER_H
#define ECIDS_CORE_POSTPROCESS_EDGEFITTER_H

#pragma once

#include "ecids_core/common/Types.h"
#include <vector>

namespace ecids_core {

class EdgeFitter {
public:
    // Basic least-squares line fit
    Line2D fit_line(const std::vector<Point2D>& points);

    // Hybrid RANSAC: fit line, correct inliers, filter outliers, estimate missing
    Line2D hybrid_ransac_fit(const std::vector<Point2D>& points,
                             double inlier_threshold = 5.0,
                             int max_iterations = 100);

    // Fit from detections — original 2-edge API (backward compat)
    Line2D fit_up_edge(const std::vector<Detection>& up_cleats);
    Line2D fit_dn_edge(const std::vector<Detection>& dn_cleats);

    // 4-edge fitting from classified cleats
    Line2D fit_up_long_edge(const std::vector<ClassifiedCleat>& up_long_cleats);
    Line2D fit_up_short_edge(const std::vector<ClassifiedCleat>& up_short_cleats);
    Line2D fit_dn_long_edge(const std::vector<ClassifiedCleat>& dn_long_cleats);
    Line2D fit_dn_short_edge(const std::vector<ClassifiedCleat>& dn_short_cleats);

    std::vector<Point2D> extract_edge_points(const Detection& det, bool bottom_edge);
    std::vector<Point2D> extract_classified_bottom_points(const std::vector<ClassifiedCleat>& cleats);
    std::vector<Point2D> extract_classified_top_points(const std::vector<ClassifiedCleat>& cleats);

    Point2D line_intersection_x(const Line2D& line, double x);
    double point_to_line_distance(const Point2D& p, const Line2D& line);

    // Estimate missing cleat positions based on expected spacing
    std::vector<Point2D> estimate_missing_points(
        const std::vector<Point2D>& existing,
        double expected_spacing,
        double x_start, double x_end,
        const Line2D& reference_line);

private:
    Point2D centroid_(const std::vector<Point2D>& pts);
    std::vector<Point2D> remove_outliers_(std::vector<Point2D> pts, const Line2D& line, double threshold = 2.0);
};

} // namespace ecids_core

#endif // ECIDS_CORE_POSTPROCESS_EDGEFITTER_H
