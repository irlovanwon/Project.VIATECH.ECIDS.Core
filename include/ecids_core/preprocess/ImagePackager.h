/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Package image for AI (File/Http/Binary mode)
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented File/Http/Binary packaging
 */

#ifndef ECIDS_CORE_PREPROCESS_IMAGEPACKAGER_H
#define ECIDS_CORE_PREPROCESS_IMAGEPACKAGER_H

#pragma once

#include "ecids_core/common/Types.h"
#include <string>
#include <vector>
#include <cstdint>

namespace ecids_core {

struct PackageResult {
    std::vector<std::string> uris;
    std::vector<std::string> filenames;
    std::vector<std::vector<uint8_t>> binary_data;
    bool is_binary = false;
};

class ImagePackager {
public:
    void init(AIMode mode, const std::string& record_path,
              const std::string& image_format = "jpg", int quality = 95);

    PackageResult package(const std::string& timestamp,
                          const uint8_t* left_data, size_t left_size,
                          const uint8_t* right_data, size_t right_size);

    AIMode mode() const { return mode_; }

private:
    AIMode mode_ = AIMode::File;
    std::string record_path_;
    std::string image_format_ = "jpg";
    int quality_ = 95;
};

} // namespace ecids_core

#endif // ECIDS_CORE_PREPROCESS_IMAGEPACKAGER_H
