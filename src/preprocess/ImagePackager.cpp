/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: ImagePackager implementation
 * Date: 2026-06-18
 * Modification: 2026-06-23 Removed RecordManager dependency, direct file write
 */

#include "ecids_core/preprocess/ImagePackager.h"
#include "ecids_core/common/Logger.h"

#include <opencv2/imgcodecs.hpp>
#include <filesystem>
#include <fstream>

namespace ecids_core {

namespace fs = std::filesystem;

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

        std::string img_dir = record_path_ + "/images";
        fs::create_directories(img_dir);
        std::string filename = timestamp + "-" + part + "." + image_format_;
        std::string filepath = img_dir + "/" + filename;
        std::ofstream ofs(filepath, std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(encoded.data()),
                  static_cast<std::streamsize>(encoded.size()));
        return filepath;
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
