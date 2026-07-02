/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: RegionLocalizer implementation — 3-region separation
 * Date: 2026-07-02
 */

#include "ecids_core/postprocess/RegionLocalizer.h"
#include <algorithm>

namespace ecids_core {

RegionInfo RegionLocalizer::localize(
    const std::vector<Detection>& gaps,
    const std::vector<Detection>& up_cleats,
    const std::vector<Detection>& dn_cleats,
    const std::vector<Detection>& riser_cleats,
    int image_height) {

    RegionInfo info;

    double gap_min_y = 1e18, gap_max_y = 0;
    for (const auto& g : gaps) {
        for (const auto& c : g.coordinates) {
            if (c.second < gap_min_y) gap_min_y = c.second;
            if (c.second > gap_max_y) gap_max_y = c.second;
        }
    }

    if (gap_min_y >= 1e18) {
        return localize_from_gap(image_height / 2.0, image_height * 0.1, image_height);
    }

    info.gap_y_start = gap_min_y;
    info.gap_y_end = gap_max_y;
    info.up_y_start = 0;
    info.up_y_end = gap_min_y;
    info.dn_y_start = gap_max_y;
    info.dn_y_end = image_height;
    info.valid = true;
    return info;
}

RegionInfo RegionLocalizer::localize_from_gap(double gap_center_y, double gap_height,
                                               int image_height, double margin_ratio) {
    RegionInfo info;
    double half_gap = gap_height / 2.0;
    info.gap_y_start = gap_center_y - half_gap;
    info.gap_y_end = gap_center_y + half_gap;
    info.up_y_start = 0;
    info.up_y_end = info.gap_y_start;
    info.dn_y_start = info.gap_y_end;
    info.dn_y_end = image_height;
    info.valid = true;
    return info;
}

} // namespace ecids_core
