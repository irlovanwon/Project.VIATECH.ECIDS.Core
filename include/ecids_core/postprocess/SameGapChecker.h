/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Check if current gap is the same as previous gap
 * Date: 2026-07-02
 */

#ifndef ECIDS_CORE_POSTPROCESS_SAMEGAPCHECKER_H
#define ECIDS_CORE_POSTPROCESS_SAMEGAPCHECKER_H

#pragma once

#include "ecids_core/common/Types.h"

namespace ecids_core {

struct SameGapConfig {
    bool enabled = true;
    double location_threshold = 30.0;  // max pixel distance to be "same"
    double size_threshold = 0.3;       // max relative size difference
};

class SameGapChecker {
public:
    void init(const SameGapConfig& config) { config_ = config; }

    // Returns true if current gap is the same as previous gap
    bool is_same_gap(const Detection& current_gap);

    void reset() { has_previous_ = false; }

private:
    SameGapConfig config_;
    bool has_previous_ = false;
    double prev_center_x_ = 0.0;
    double prev_center_y_ = 0.0;
    double prev_width_ = 0.0;
    double prev_height_ = 0.0;
};

} // namespace ecids_core

#endif // ECIDS_CORE_POSTPROCESS_SAMEGAPCHECKER_H
