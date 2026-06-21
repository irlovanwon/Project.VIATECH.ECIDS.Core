/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: JSON config load/save/sync
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented ConfigManager with nlohmann_json
 */

#ifndef ECIDS_CORE_DATA_CONFIGMANAGER_H
#define ECIDS_CORE_DATA_CONFIGMANAGER_H

#pragma once

#include <string>
#include <mutex>

namespace ecids_core {

class ConfigManager {
public:
    static ConfigManager& instance();

    bool load(const std::string& path);
    bool save();
    bool save_as(const std::string& path);

    std::string get_string(const std::string& key, const std::string& default_val = "") const;
    int get_int(const std::string& key, int default_val = 0) const;
    double get_double(const std::string& key, double default_val = 0.0) const;
    bool get_bool(const std::string& key, bool default_val = false) const;

    void set_string(const std::string& key, const std::string& val);
    void set_int(const std::string& key, int val);
    void set_double(const std::string& key, double val);
    void set_bool(const std::string& key, bool val);

    std::string config_path() const { return config_path_; }
    std::string dump() const;

private:
    ConfigManager() = default;

    std::string config_path_;
    mutable std::mutex mutex_;
};

} // namespace ecids_core

#endif // ECIDS_CORE_DATA_CONFIGMANAGER_H
