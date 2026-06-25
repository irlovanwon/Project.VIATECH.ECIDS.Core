/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Create/manage inspection and AI test records
 * Date: 2026-06-18
 * Modification: 2026-06-23 New subfolder structure (installation/, inspection/), new filename convention
 */

#ifndef ECIDS_CORE_DATABASE_RECORDMANAGER_H
#define ECIDS_CORE_DATABASE_RECORDMANAGER_H

#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace ecids_core {

class RecordManager {
public:
    void init(const std::string& root_path);

    std::string create_inspection_record(const std::string& station_id,
                                         const std::string& escalator_id,
                                         const std::string& task_id);

    std::string create_ai_test_record(const std::string& station_id = "",
                                            const std::string& escalator_id = "");

    std::string save_image(const std::string& record_path,
                           const std::string& subfolder,
                           const std::string& camera_id,
                           int pair_index,
                           const uint8_t* data, size_t size,
                           const std::string& format = "jpg");

    void save_ai_result(const std::string& record_path,
                        const std::string& subfolder,
                        const std::string& camera_id,
                        int pair_index,
                        const std::string& json_str);

    void save_stereo_result(const std::string& record_path,
                            const std::string& subfolder,
                            const std::string& camera_id,
                            int pair_index,
                            const std::string& json_str);

    void save_pointcloud(const std::string& record_path,
                         const std::string& subfolder,
                         int pair_index,
                         const uint8_t* data, size_t size);

    void save_depth(const std::string& record_path,
                    const std::string& subfolder,
                    int pair_index,
                    const uint8_t* data, size_t size);

    const std::string& root_path() const { return root_path_; }
    std::string active_record() const { return active_record_; }
    void set_active_record(const std::string& path) { active_record_ = path; }

    static double calculate_size_gb(const std::string& root_path);

private:
    std::string root_path_;
    std::string active_record_;

    static void ensure_subdirs_(const std::string& record_path);
    static void ensure_dir_(const std::string& path);
    static void write_file_(const std::string& path, const uint8_t* data, size_t size);
    static void write_text_(const std::string& path, const std::string& content);
    static std::string format_pair_index_(int idx);
};

} // namespace ecids_core

#endif // ECIDS_CORE_DATABASE_RECORDMANAGER_H
