/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: EdgeFitter — hybrid RANSAC + 4-edge fitting
 * Date: 2026-06-18
 * Modification: 2026-07-02 Added hybrid RANSAC, 4-edge fitting, missing point estimation
 */

#include "ecids_core/postprocess/EdgeFitter.h"
#include <cmath>
#include <algorithm>
#include <random>

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

double EdgeFitter::point_to_line_distance(const Point2D& p, const Line2D& line) {
    if (!line.valid) return 1e18;
    double predicted_y = line.slope * p.x + line.intercept;
    return std::abs(p.y - predicted_y) / std::sqrt(1.0 + line.slope * line.slope);
}

Line2D EdgeFitter::hybrid_ransac_fit(const std::vector<Point2D>& points,
                                      double inlier_threshold,
                                      int max_iterations) {
    Line2D best_line;
    if (points.size() < 2) return best_line;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, static_cast<int>(points.size()) - 1);

    int best_inliers = 0;

    int actual_iters = std::min(max_iterations, static_cast<int>(points.size() * (points.size() - 1) / 2));

    for (int i = 0; i < actual_iters; ++i) {
        int a = dist(gen);
        int b = dist(gen);
        if (a == b) continue;

        Line2D candidate;
        double dx = points[a].x - points[b].x;
        double dy = points[a].y - points[b].y;
        if (std::abs(dx) < 1e-10) continue;
        candidate.slope = dy / dx;
        candidate.intercept = points[a].y - candidate.slope * points[a].x;
        candidate.valid = true;

        int inliers = 0;
        for (const auto& p : points) {
            if (point_to_line_distance(p, candidate) < inlier_threshold) {
                ++inliers;
            }
        }

        if (inliers > best_inliers) {
            best_inliers = inliers;
            best_line = candidate;
        }
    }

    if (best_inliers >= 2) {
        std::vector<Point2D> inlier_pts;
        for (const auto& p : points) {
            if (point_to_line_distance(p, best_line) < inlier_threshold) {
                inlier_pts.push_back(p);
            }
        }
        if (inlier_pts.size() >= 2) {
            best_line = fit_line(inlier_pts);
        }
    } else if (points.size() >= 2) {
        best_line = fit_line(points);
    }

    return best_line;
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
        if (point_to_line_distance(p, line) <= threshold) {
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
    } else {
        std::sort(pts.begin(), pts.end(),
                  [](const Point2D& a, const Point2D& b) { return a.y < b.y; });
    }

    size_t half = pts.size() / 2;
    for (size_t i = 0; i < half && i < pts.size(); ++i) {
        result.push_back(pts[i]);
    }
    return result;
}

std::vector<Point2D> EdgeFitter::extract_classified_bottom_points(
    const std::vector<ClassifiedCleat>& cleats) {
    std::vector<Point2D> pts;
    for (const auto& cc : cleats) {
        pts.push_back(cc.bottom_mid);
    }
    return pts;
}

std::vector<Point2D> EdgeFitter::extract_classified_top_points(
    const std::vector<ClassifiedCleat>& cleats) {
    std::vector<Point2D> pts;
    for (const auto& cc : cleats) {
        Point2D p;
        p.x = cc.center_x;
        p.y = cc.top_y;
        pts.push_back(p);
    }
    return pts;
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

Line2D EdgeFitter::fit_up_long_edge(const std::vector<ClassifiedCleat>& up_long_cleats) {
    auto pts = extract_classified_bottom_points(up_long_cleats);
    if (pts.empty()) return Line2D{};
    return hybrid_ransac_fit(pts);
}

Line2D EdgeFitter::fit_up_short_edge(const std::vector<ClassifiedCleat>& up_short_cleats) {
    auto pts = extract_classified_bottom_points(up_short_cleats);
    if (pts.empty()) return Line2D{};
    return hybrid_ransac_fit(pts);
}

Line2D EdgeFitter::fit_dn_long_edge(const std::vector<ClassifiedCleat>& dn_long_cleats) {
    auto pts = extract_classified_top_points(dn_long_cleats);
    if (pts.empty()) return Line2D{};
    return hybrid_ransac_fit(pts);
}

Line2D EdgeFitter::fit_dn_short_edge(const std::vector<ClassifiedCleat>& dn_short_cleats) {
    auto pts = extract_classified_top_points(dn_short_cleats);
    if (pts.empty()) return Line2D{};
    return hybrid_ransac_fit(pts);
}

Point2D EdgeFitter::line_intersection_x(const Line2D& line, double x) {
    Point2D p;
    p.x = x;
    if (line.valid) {
        p.y = line.slope * x + line.intercept;
    }
    return p;
}

std::vector<Point2D> EdgeFitter::estimate_missing_points(
    const std::vector<Point2D>& existing,
    double expected_spacing,
    double x_start, double x_end,
    const Line2D& reference_line) {

    std::vector<Point2D> result = existing;
    if (!reference_line.valid || expected_spacing <= 0) return result;

    std::vector<Point2D> sorted_pts = existing;
    std::sort(sorted_pts.begin(), sorted_pts.end(),
              [](const Point2D& a, const Point2D& b) { return a.x < b.x; });

    for (double x = x_start; x <= x_end; x += expected_spacing) {
        bool found = false;
        for (const auto& p : sorted_pts) {
            if (std::abs(p.x - x) < expected_spacing * 0.3) {
                found = true;
                break;
            }
        }
        if (!found) {
            Point2D missing;
            missing.x = x;
            missing.y = reference_line.slope * x + reference_line.intercept;
            result.push_back(missing);
        }
    }

    return result;
}

} // namespace ecids_core
