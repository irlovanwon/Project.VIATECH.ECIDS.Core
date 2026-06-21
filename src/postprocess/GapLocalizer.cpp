/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: GapLocalizer implementation
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented
 */

#include "ecids_core/postprocess/GapLocalizer.h"
#include <cmath>
#include <algorithm>

namespace ecids_core {

Point2D GapLocalizer::perpendicular_to_dn_(const Line2D& up, const Line2D& dn,
                                            const Point2D& start) {
    if (!up.valid || !dn.valid) return start;

    double up_angle = std::atan(up.slope);
    double perp_dx = std::cos(up_angle + M_PI / 2.0);
    double perp_dy = std::sin(up_angle + M_PI / 2.0);

    if (perp_dy >= 0) { perp_dx = -perp_dx; perp_dy = -perp_dy; }

    Point2D result = start;
    double step = 1.0;
    for (int i = 0; i < 10000; ++i) {
        result.x += perp_dx * step;
        result.y += perp_dy * step;

        double dn_y = dn.slope * result.x + dn.intercept;
        if (result.y <= dn_y) break;
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
        gl.dn_point = perpendicular_to_dn_(up_edge, dn_edge, mid_up);
        double dx = gl.dn_point.x - gl.up_point.x;
        double dy = gl.dn_point.y - gl.up_point.y;
        gl.length_px = std::sqrt(dx * dx + dy * dy);
    }

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

} // namespace ecids_core
