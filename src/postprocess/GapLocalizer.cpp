/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: GapLocalizer — up/down gap line localization with 4 edges
 * Date: 2026-06-18
 * Modification: 2026-07-02 Added up gap lines (up_short→dn_long) + down gap lines (up_long→dn_short)
 */

#include "ecids_core/postprocess/GapLocalizer.h"
#include <cmath>
#include <algorithm>

namespace ecids_core {

Point2D GapLocalizer::perpendicular_to_line_(const Line2D& start_edge, const Line2D& end_edge,
                                              const Point2D& start) {
    if (!start_edge.valid || !end_edge.valid) return start;

    double angle = std::atan(start_edge.slope);
    double perp_dx = std::cos(angle + M_PI / 2.0);
    double perp_dy = std::sin(angle + M_PI / 2.0);

    // Ensure we go downward (increasing y)
    if (perp_dy < 0) { perp_dx = -perp_dx; perp_dy = -perp_dy; }

    Point2D result = start;
    for (int i = 0; i < 10000; ++i) {
        result.x += perp_dx;
        result.y += perp_dy;

        double end_y = end_edge.slope * result.x + end_edge.intercept;
        if (result.y >= end_y) break;
    }
    return result;
}

GapLine GapLocalizer::locate_gap(const Line2D& up_edge, const Line2D& dn_edge,
                                  const std::vector<Detection>& gaps) {
    GapLine gl;
    if (!up_edge.valid) return gl;

    Point2D mid_up;
    if (gaps.empty()) {
        mid_up.x = 0;
        mid_up.y = up_edge.intercept;
    } else {
        const auto& coords = gaps[0].coordinates;
        if (coords.empty()) {
            mid_up.x = 0;
            mid_up.y = up_edge.intercept;
        } else {
            double sx = 0;
            for (const auto& c : coords) sx += c.first;
            mid_up.x = sx / coords.size();
            mid_up.y = up_edge.slope * mid_up.x + up_edge.intercept;
        }
    }

    gl.up_point = mid_up;

    if (dn_edge.valid) {
        gl.dn_point = perpendicular_to_line_(up_edge, dn_edge, mid_up);
        double dx = gl.dn_point.x - gl.up_point.x;
        double dy = gl.dn_point.y - gl.up_point.y;
        gl.length_px = std::sqrt(dx * dx + dy * dy);
    }

    gl.type = GapLineType::DownGap;
    gl.valid = true;
    return gl;
}

std::vector<GapLine> GapLocalizer::locate_all_gaps(const Line2D& up_edge,
                                                    const Line2D& dn_edge,
                                                    const std::vector<Detection>& gaps) {
    std::vector<GapLine> result;
    for (const auto& gap : gaps) {
        std::vector<Detection> single = {gap};
        auto gl = locate_gap(up_edge, dn_edge, single);
        if (gl.valid) result.push_back(gl);
    }
    if (result.empty()) {
        auto gl = locate_gap(up_edge, dn_edge, {});
        if (gl.valid) result.push_back(gl);
    }
    return result;
}

std::vector<GapLine> GapLocalizer::locate_up_gap_lines(
    const Line2D& up_short, const Line2D& dn_long,
    const std::vector<ClassifiedCleat>& up_short_cleats) {

    std::vector<GapLine> result;

    if (!up_short.valid || !dn_long.valid) return result;

    for (const auto& cc : up_short_cleats) {
        GapLine gl;
        gl.type = GapLineType::UpGap;

        // Start from middle point of bottom of up cleat short
        gl.up_point = cc.bottom_mid;

        // Adjust to up_short edge line
        gl.up_point.y = up_short.slope * gl.up_point.x + up_short.intercept;

        // Extend perpendicular to up_short until reaching dn_long
        gl.dn_point = perpendicular_to_line_(up_short, dn_long, gl.up_point);

        double dx = gl.dn_point.x - gl.up_point.x;
        double dy = gl.dn_point.y - gl.up_point.y;
        gl.length_px = std::sqrt(dx * dx + dy * dy);
        gl.valid = true;

        result.push_back(gl);
    }

    return result;
}

std::vector<GapLine> GapLocalizer::locate_down_gap_lines(
    const Line2D& up_long, const Line2D& dn_short,
    const std::vector<ClassifiedCleat>& up_long_cleats) {

    std::vector<GapLine> result;

    if (!up_long.valid || !dn_short.valid) return result;

    for (const auto& cc : up_long_cleats) {
        GapLine gl;
        gl.type = GapLineType::DownGap;

        // Start from middle point of bottom of up cleat long
        gl.up_point = cc.bottom_mid;

        // Adjust to up_long edge line
        gl.up_point.y = up_long.slope * gl.up_point.x + up_long.intercept;

        // Extend perpendicular to up_long until reaching dn_short
        gl.dn_point = perpendicular_to_line_(up_long, dn_short, gl.up_point);

        double dx = gl.dn_point.x - gl.up_point.x;
        double dy = gl.dn_point.y - gl.up_point.y;
        gl.length_px = std::sqrt(dx * dx + dy * dy);
        gl.valid = true;

        result.push_back(gl);
    }

    return result;
}

} // namespace ecids_core
