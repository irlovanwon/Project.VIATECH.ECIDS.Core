/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: DatabaseManager implementation
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented
 */

#include "ecids_core/database/DatabaseManager.h"
#include "ecids_core/common/Logger.h"

namespace ecids_core {

DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager inst;
    return inst;
}

void DatabaseManager::init(const std::string& root_path, bool enable_housekeep,
                           double max_size_gb) {
    root_path_ = root_path;
    record_mgr_.init(root_path);

    if (enable_housekeep) {
        housekeeper_.set_size_provider([this]() {
            return RecordManager::calculate_size_gb(root_path_);
        });
        housekeeper_.start(root_path, max_size_gb);
    }

    initialized_ = true;
    Logger::info("DatabaseManager: initialized root=" + root_path);
}

double DatabaseManager::current_size_gb() const {
    return RecordManager::calculate_size_gb(root_path_);
}

std::string DatabaseManager::root_path() const {
    return root_path_;
}

} // namespace ecids_core
