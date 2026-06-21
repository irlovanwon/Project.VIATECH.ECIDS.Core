/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: ImagePackager implementation
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented
 */

#include "ecids_core/preprocess/ImagePackager.h"
#include "ecids_core/database/RecordManager.h"
#include "ecids_core/common/Logger.h"

#include <opencv2/imgcodecs.hpp>

namespace ecids_core {

void ImagePackager::init(AIMode mode, const std::string& record_path,
                         const std::string& image_format, int quality) {
    mode_ = mode;
    record_path_ = record_path;
    image_format_ = image_format;
    quality_ = quality;
}

PackageResult ImagePackager::package(const std::string& timestamp,
                                     const uint8_t* left_data, size_t left_size,
                                     const uint8_t* right_data, size_t right_size) {
    PackageResult result;

    if (mode_ == AIMode::Binary) {
        result.binary_data.emplace_back(left_data, left_data + left_size);
        result.binary_data.emplace_back(right_data, right_data + right_size);
        result.is_binary = true;
        result.filenames.push_back(timestamp + "-L.bin");
        result.filenames.push_back(timestamp + "-R.bin");
        return result;
    }

    std::vector<int> params;
    if (image_format_ == "jpg") {
        params = {cv::IMWRITE_JPEG_QUALITY, quality_};
    }

    auto save_image = [&](const uint8_t* raw, size_t sz, const std::string& part) -> std::string {
        std::vector<uint8_t> raw_vec(raw, raw + sz);

        cv::Mat img;
        int type = (sz % 4 == 0) ? CV_8UC4 : CV_8UC3;
        int w = (sz % 4 == 0) ? static_cast<int>(sz / 4) : static_cast<int>(sz / 3);
        int h = 1;
        if (w > 1080) { h = w / 1280; w = 1280; }
        if (w > 0 && h > 0 && static_cast<size_t>(w) * h * (type == CV_8UC4 ? 4 : 3) == sz) {
            img = cv::Mat(h, w, type, raw_vec.data()).clone();
        }

        std::vector<uint8_t> encoded;
        if (!img.empty()) {
            cv::imencode("." + image_format_, img, encoded, params);
        } else {
            encoded = raw_vec;
        }

        std::string filename = RecordManager().save_image(
            record_path_, timestamp, part, encoded.data(), encoded.size(), image_format_);
        return record_path_ + "/images/" + filename;
    };

    std::string left_path = save_image(left_data, left_size, "L");
    std::string right_path = save_image(right_data, right_size, "R");

    result.uris.push_back(left_path);
    result.uris.push_back(right_path);
    result.filenames.push_back(timestamp + "-L." + image_format_);
    result.filenames.push_back(timestamp + "-R." + image_format_);
    result.is_binary = false;

    return result;
}

} // namespace ecids_core
