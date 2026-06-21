/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: EdgeFitter implementation
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented
 */

#include "ecids_core/postprocess/EdgeFitter.h"
#include <cmath>
#include <algorithm>

namespace ecids_core {

Line2D EdgeFitter::fit_line(const std::vector<Point2D>& points) {
    Line2D line;
    if (points.size() < 2) return line;

    double n = static_cast<double>(points.size());
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0;

    for (const auto& p : points) {
        sum_x += p.x;
        sum_y += p.y;
        sum_xy += p.x * p.y;
        sum_xx += p.x * p.x;
    }

    double denom = n * sum_xx - sum_x * sum_x;
    if (std::abs(denom) < 1e-10) return line;

    line.slope = (n * sum_xy - sum_x * sum_y) / denom;
    line.intercept = (sum_y - line.slope * sum_x) / n;
    line.valid = true;
    return line;
}

Point2D EdgeFitter::centroid_(const std::vector<Point2D>& pts) {
    Point2D c;
    for (const auto& p : pts) { c.x += p.x; c.y += p.y; }
    if (!pts.empty()) { c.x /= pts.size(); c.y /= pts.size(); }
    return c;
}

std::vector<Point2D> EdgeFitter::remove_outliers_(std::vector<Point2D> pts,
                                                    const Line2D& line,
                                                    double threshold) {
    if (!line.valid) return pts;

    std::vector<Point2D> result;
    for (const auto& p : pts) {
        double predicted_y = line.slope * p.x + line.intercept;
        double residual = std::abs(p.y - predicted_y);
        if (residual <= threshold * std::sqrt(1.0 + line.slope * line.slope)) {
            result.push_back(p);
        }
    }
    return result.empty() ? pts : result;
}

std::vector<Point2D> EdgeFitter::extract_edge_points(const Detection& det, bool bottom_edge) {
    std::vector<Point2D> result;
    if (det.coordinates.size() < 2) return result;

    std::vector<Point2D> pts;
    for (const auto& c : det.coordinates) {
        pts.push_back({c.first, c.second});
    }

    if (bottom_edge) {
        std::sort(pts.begin(), pts.end(),
                  [](const Point2D& a, const Point2D& b) { return a.y > b.y; });

        size_t half = pts.size() / 2;
        for (size_t i = 0; i < half && i < pts.size(); ++i) {
            result.push_back(pts[i]);
        }
    } else {
        std::sort(pts.begin(), pts.end(),
                  [](const Point2D& a, const Point2D& b) { return a.y < b.y; });

        size_t half = pts.size() / 2;
        for (size_t i = 0; i < half && i < pts.size(); ++i) {
            result.push_back(pts[i]);
        }
    }
    return result;
}

Line2D EdgeFitter::fit_up_edge(const std::vector<Detection>& up_cleats) {
    std::vector<Point2D> edge_points;
    for (const auto& det : up_cleats) {
        auto pts = extract_edge_points(det, true);
        edge_points.insert(edge_points.end(), pts.begin(), pts.end());
    }
    if (edge_points.empty()) return Line2D{};

    Line2D first = fit_line(edge_points);
    if (first.valid) {
        auto cleaned = remove_outliers_(std::move(edge_points), first);
        return fit_line(cleaned);
    }
    return first;
}

Line2D EdgeFitter::fit_dn_edge(const std::vector<Detection>& dn_cleats) {
    std::vector<Point2D> edge_points;
    for (const auto& det : dn_cleats) {
        auto pts = extract_edge_points(det, false);
        edge_points.insert(edge_points.end(), pts.begin(), pts.end());
    }
    if (edge_points.empty()) return Line2D{};

    Line2D first = fit_line(edge_points);
    if (first.valid) {
        auto cleaned = remove_outliers_(std::move(edge_points), first);
        return fit_line(cleaned);
    }
    return first;
}

Point2D EdgeFitter::line_intersection_x(const Line2D& line, double x) {
    Point2D p;
    p.x = x;
    if (line.valid) {
        p.y = line.slope * x + line.intercept;
    }
    return p;
}

} // namespace ecids_core
