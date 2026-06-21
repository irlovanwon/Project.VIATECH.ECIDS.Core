/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: ConfigManager implementation
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented with nlohmann_json, dot-path access
 */

#include "ecids_core/data/ConfigManager.h"
#include "ecids_core/common/Logger.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>

namespace ecids_core {

using json = nlohmann::json;

static json g_config;
static std::mutex g_config_mutex;

ConfigManager& ConfigManager::instance() {
    static ConfigManager inst;
    return inst;
}

static json* navigate(json& j, const std::string& key, bool create = false) {
    json* cur = &j;
    size_t start = 0;
    while (start < key.size()) {
        size_t dot = key.find('.', start);
        std::string segment = (dot == std::string::npos)
            ? key.substr(start)
            : key.substr(start, dot - start);
        if (create && !cur->contains(segment)) {
            (*cur)[segment] = json::object();
        }
        if (!cur->contains(segment)) return nullptr;
        cur = &(*cur)[segment];
        if (dot == std::string::npos) break;
        start = dot + 1;
    }
    return cur;
}

static const json* navigate_const(const json& j, const std::string& key) {
    const json* cur = &j;
    size_t start = 0;
    while (start < key.size()) {
        size_t dot = key.find('.', start);
        std::string segment = (dot == std::string::npos)
            ? key.substr(start)
            : key.substr(start, dot - start);
        if (!cur->contains(segment)) return nullptr;
        cur = &(*cur)[segment];
        if (dot == std::string::npos) break;
        start = dot + 1;
    }
    return cur;
}

bool ConfigManager::load(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_path_ = path;

    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        Logger::error("ConfigManager: cannot open " + path);
        return false;
    }

    try {
        json j;
        ifs >> j;
        g_config = std::move(j);
        Logger::info("ConfigManager: loaded " + path);
        return true;
    } catch (const std::exception& e) {
        Logger::error(std::string("ConfigManager: parse error: ") + e.what());
        return false;
    }
}

bool ConfigManager::save() {
    return save_as(config_path_);
}

bool ConfigManager::save_as(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        Logger::error("ConfigManager: cannot write " + path);
        return false;
    }

    try {
        ofs << g_config.dump(4);
        Logger::info("ConfigManager: saved to " + path);
        return true;
    } catch (const std::exception& e) {
        Logger::error(std::string("ConfigManager: save error: ") + e.what());
        return false;
    }
}

std::string ConfigManager::get_string(const std::string& key, const std::string& default_val) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const json* node = navigate_const(g_config, key);
    if (node && node->is_string()) return node->get<std::string>();
    return default_val;
}

int ConfigManager::get_int(const std::string& key, int default_val) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const json* node = navigate_const(g_config, key);
    if (node && node->is_number_integer()) return node->get<int>();
    return default_val;
}

double ConfigManager::get_double(const std::string& key, double default_val) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const json* node = navigate_const(g_config, key);
    if (node && node->is_number()) return node->get<double>();
    return default_val;
}

bool ConfigManager::get_bool(const std::string& key, bool default_val) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const json* node = navigate_const(g_config, key);
    if (node && node->is_boolean()) return node->get<bool>();
    return default_val;
}

void ConfigManager::set_string(const std::string& key, const std::string& val) {
    std::lock_guard<std::mutex> lock(mutex_);
    json* node = navigate(g_config, key, true);
    if (node) *node = val;
}

void ConfigManager::set_int(const std::string& key, int val) {
    std::lock_guard<std::mutex> lock(mutex_);
    json* node = navigate(g_config, key, true);
    if (node) *node = val;
}

void ConfigManager::set_double(const std::string& key, double val) {
    std::lock_guard<std::mutex> lock(mutex_);
    json* node = navigate(g_config, key, true);
    if (node) *node = val;
}

void ConfigManager::set_bool(const std::string& key, bool val) {
    std::lock_guard<std::mutex> lock(mutex_);
    json* node = navigate(g_config, key, true);
    if (node) *node = val;
}

std::string ConfigManager::dump() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return g_config.dump(4);
}

} // namespace ecids_core
