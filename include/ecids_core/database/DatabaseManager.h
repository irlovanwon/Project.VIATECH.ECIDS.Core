/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Flat-file DB management — top-level coordinator
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented
 */

#ifndef ECIDS_CORE_DATABASE_DATABASEMANAGER_H
#define ECIDS_CORE_DATABASE_DATABASEMANAGER_H

#pragma once

#include "ecids_core/database/RecordManager.h"
#include "ecids_core/database/HouseKeeper.h"

namespace ecids_core {

class DatabaseManager {
public:
    static DatabaseManager& instance();

    void init(const std::string& root_path, bool enable_housekeep,
              double max_size_gb);

    RecordManager& records() { return record_mgr_; }
    HouseKeeper& housekeeper() { return housekeeper_; }

    double current_size_gb() const;
    std::string root_path() const;

private:
    DatabaseManager() = default;

    RecordManager record_mgr_;
    HouseKeeper housekeeper_;
    std::string root_path_;
    bool initialized_ = false;
};

} // namespace ecids_core

#endif // ECIDS_CORE_DATABASE_DATABASEMANAGER_H
