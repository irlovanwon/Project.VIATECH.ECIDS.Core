/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: RecordManager implementation
 * Date: 2026-06-18
 * Modification: 2026-06-23 New subfolder structure, new filename convention
 */

#include "ecids_core/database/RecordManager.h"
#include "ecids_core/common/Timestamp.h"
#include "ecids_core/common/Logger.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <fstream>
#include <filesystem>
#include <cstdio>
#include <iomanip>
#include <sstream>

namespace ecids_core {

namespace fs = std::filesystem;

void RecordManager::init(const std::string& root_path) {
    root_path_ = root_path;
    ensure_dir_(root_path_);
    Logger::info("RecordManager: root=" + root_path_);
}

void RecordManager::ensure_dir_(const std::string& path) {
    fs::create_directories(path);
}

void RecordManager::ensure_subdirs_(const std::string& record_path) {
    ensure_dir_(record_path + "/installation");
    ensure_dir_(record_path + "/inspection");
}

std::string RecordManager::format_pair_index_(int idx) {
    std::ostringstream ss;
    ss << std::setfill('0') << std::setw(3) << idx;
    return ss.str();
}

std::string RecordManager::create_inspection_record(const std::string& station_id,
                                                     const std::string& escalator_id,
                                                     const std::string& task_id) {
    auto parts = Timestamp::now_parts();
    std::string date_path = Timestamp::db_date_path_from_parts(parts);
    std::string folder_part = Timestamp::db_folder_part_from_parts(parts);

    std::string record_name = "(Inspection)" + folder_part
                              + "-" + station_id
                              + "-" + escalator_id
                              + "-" + task_id;
    std::string full_path = root_path_ + "/" + date_path + "/" + record_name;
    ensure_subdirs_(full_path);
    active_record_ = full_path;

    Logger::info("RecordManager: created inspection record " + record_name);
    return full_path;
}

std::string RecordManager::create_ai_test_record() {
    auto parts = Timestamp::now_parts();
    std::string date_path = Timestamp::db_date_path_from_parts(parts);
    std::string folder_part = Timestamp::db_folder_part_from_parts(parts);

    std::string record_name = "(AI Test)" + folder_part;
    std::string full_path = root_path_ + "/" + date_path + "/" + record_name;
    ensure_dir_(full_path);
    active_record_ = full_path;

    Logger::info("RecordManager: created AI test record " + record_name);
    return full_path;
}

std::string RecordManager::save_image(const std::string& record_path,
                                      const std::string& subfolder,
                                      const std::string& camera_id,
                                      int pair_index,
                                      const uint8_t* data, size_t size,
                                      const std::string& format) {
    std::string filename = camera_id + "_" + format_pair_index_(pair_index) + "." + format;
    std::string dir = record_path + "/" + subfolder;
    ensure_dir_(dir);
    std::string filepath = dir + "/" + filename;
    write_file_(filepath, data, size);
    return filename;
}

void RecordManager::save_ai_result(const std::string& record_path,
                                   const std::string& subfolder,
                                   const std::string& camera_id,
                                   int pair_index,
                                   const std::string& json_str) {
    std::string filename = "AI_" + camera_id + "_" + format_pair_index_(pair_index) + ".json";
    std::string dir = record_path + "/" + subfolder;
    ensure_dir_(dir);
    write_text_(dir + "/" + filename, json_str);
}

void RecordManager::save_stereo_result(const std::string& record_path,
                                       const std::string& subfolder,
                                       const std::string& camera_id,
                                       int pair_index,
                                       const std::string& json_str) {
    (void)camera_id;
    std::string filename = "Stereo_" + format_pair_index_(pair_index) + ".json";
    std::string dir = record_path + "/" + subfolder;
    ensure_dir_(dir);
    write_text_(dir + "/" + filename, json_str);
}

void RecordManager::save_pointcloud(const std::string& record_path,
                                    const std::string& subfolder,
                                    int pair_index,
                                    const uint8_t* data, size_t size) {
    std::string filename = "PC_" + format_pair_index_(pair_index) + ".pcd";
    std::string dir = record_path + "/" + subfolder;
    ensure_dir_(dir);
    write_file_(dir + "/" + filename, data, size);
}

void RecordManager::save_depth(const std::string& record_path,
                               const std::string& subfolder,
                               int pair_index,
                               const uint8_t* data, size_t size) {
    std::string filename = "Depth_" + format_pair_index_(pair_index) + ".dat";
    std::string dir = record_path + "/" + subfolder;
    ensure_dir_(dir);
    write_file_(dir + "/" + filename, data, size);
}

void RecordManager::write_file_(const std::string& path, const uint8_t* data, size_t size) {
    std::ofstream ofs(path, std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
}

void RecordManager::write_text_(const std::string& path, const std::string& content) {
    std::ofstream ofs(path);
    ofs << content;
}

double RecordManager::calculate_size_gb(const std::string& root_path) {
    if (!fs::exists(root_path)) return 0.0;

    uintmax_t total = 0;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(root_path, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) continue;
        std::error_code fec;
        if (it->is_regular_file(fec)) {
            total += it->file_size(fec);
        }
    }
    return static_cast<double>(total) / (1024.0 * 1024.0 * 1024.0);
}

} // namespace ecids_core
