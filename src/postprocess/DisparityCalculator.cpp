/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: DisparityCalculator implementation
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented
 */

#include "ecids_core/postprocess/DisparityCalculator.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <cmath>
#include <algorithm>

namespace ecids_core {

void DisparityCalculator::init(double baseline_mm, double focal_length_px) {
    baseline_mm_ = baseline_mm;
    focal_length_px_ = focal_length_px;
}

double DisparityCalculator::template_match_(const uint8_t* left, int lw, int lh,
                                             const uint8_t* right, int rw, int rh,
                                             int px, int py, int wr) {
    int x0 = std::max(0, px - wr);
    int y0 = std::max(0, py - wr);
    int x1 = std::min(lw, px + wr + 1);
    int y1 = std::min(lh, py + wr + 1);
    int tw = x1 - x0;
    int th = y1 - y0;
    if (tw < 3 || th < 3) return 0.0;

    int best_disp = 0;
    double best_sad = 1e18;
    int max_disp = std::min(128, rw);

    for (int d = 0; d <= max_disp; ++d) {
        if (x0 - d < 0) break;
        double sad = 0;
        int count = 0;
        for (int y = 0; y < th; ++y) {
            for (int x = 0; x < tw; ++x) {
                int li = ((y0 + y) * lw + (x0 + x)) * 4;
                int ri = ((y0 + y) * rw + (x0 + x - d)) * 4;
                if (ri < 0 || ri + 3 >= rw * rh * 4) continue;
                sad += std::abs(static_cast<int>(left[li]) - static_cast<int>(right[ri]))
                     + std::abs(static_cast<int>(left[li+1]) - static_cast<int>(right[ri+1]))
                     + std::abs(static_cast<int>(left[li+2]) - static_cast<int>(right[ri+2]));
                count++;
            }
        }
        if (count > 0) {
            double norm_sad = sad / count;
            if (norm_sad < best_sad) {
                best_sad = norm_sad;
                best_disp = d;
            }
        }
    }

    return static_cast<double>(best_disp);
}

double DisparityCalculator::calculate_disparity(const uint8_t* left_data, size_t left_size,
                                                const uint8_t* right_data, size_t right_size,
                                                const Point2D& point, int window_radius) {
    if (!left_data || !right_data || left_size == 0 || right_size == 0) return 0.0;

    cv::Mat left_img;
    cv::Mat right_img;

    std::vector<uint8_t> lvec(left_data, left_data + left_size);
    std::vector<uint8_t> rvec(right_data, right_data + right_size);

    left_img = cv::imdecode(lvec, cv::IMREAD_UNCHANGED);
    right_img = cv::imdecode(rvec, cv::IMREAD_UNCHANGED);

    if (left_img.empty() || right_img.empty()) {
        int ch = 4;
        int w = static_cast<int>(left_size / ch / 1);
        if (w == 0) w = 1;
        int h = static_cast<int>(left_size / ch / w);
        if (h == 0) return 0.0;
        left_img = cv::Mat(h, w, CV_8UC4, const_cast<uint8_t*>(left_data)).clone();
        right_img = cv::Mat(h, w, CV_8UC4, const_cast<uint8_t*>(right_data)).clone();
    }

    int px = static_cast<int>(std::round(point.x));
    int py = static_cast<int>(std::round(point.y));
    px = std::max(0, std::min(px, left_img.cols - 1));
    py = std::max(0, std::min(py, left_img.rows - 1));

    return template_match_(left_img.data, left_img.cols, left_img.rows,
                           right_img.data, right_img.cols, right_img.rows,
                           px, py, window_radius);
}

double DisparityCalculator::calculate_distance(double disparity_px) const {
    if (disparity_px < 0.1) return 0.0;
    if (focal_length_px_ < 0.1) return 0.0;
    return (baseline_mm_ * focal_length_px_) / disparity_px;
}

} // namespace ecids_core
