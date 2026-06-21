/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: StatusTracker implementation
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented
 */

#include "ecids_core/data/StatusTracker.h"

#include <nlohmann/json.hpp>

namespace ecids_core {

StatusTracker& StatusTracker::instance() {
    static StatusTracker inst;
    return inst;
}

Mode StatusTracker::active_mode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return mode_;
}

void StatusTracker::set_mode(Mode m) {
    std::lock_guard<std::mutex> lock(mutex_);
    mode_ = m;
}

bool StatusTracker::task_active() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return task_active_;
}

void StatusTracker::set_task_active(bool active) {
    std::lock_guard<std::mutex> lock(mutex_);
    task_active_ = active;
}

bool StatusTracker::stereo_camera_connected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sc_connected_;
}

void StatusTracker::set_stereo_camera_connected(bool v) {
    std::lock_guard<std::mutex> lock(mutex_);
    sc_connected_ = v;
}

bool StatusTracker::stereo_camera_capturing() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sc_capturing_;
}

void StatusTracker::set_stereo_camera_capturing(bool v) {
    std::lock_guard<std::mutex> lock(mutex_);
    sc_capturing_ = v;
}

bool StatusTracker::ai_dealer_connected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ai_connected_;
}

void StatusTracker::set_ai_dealer_connected(bool v) {
    std::lock_guard<std::mutex> lock(mutex_);
    ai_connected_ = v;
}

std::string StatusTracker::ai_dealer_mode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ai_mode_;
}

void StatusTracker::set_ai_dealer_mode(const std::string& mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    ai_mode_ = mode;
}

double StatusTracker::database_size_gb() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return db_size_gb_;
}

void StatusTracker::set_database_size_gb(double gb) {
    std::lock_guard<std::mutex> lock(mutex_);
    db_size_gb_ = gb;
}

bool StatusTracker::uploading() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return uploading_;
}

void StatusTracker::set_uploading(bool v) {
    std::lock_guard<std::mutex> lock(mutex_);
    uploading_ = v;
}

std::string StatusTracker::module_id() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return module_id_;
}

void StatusTracker::set_module_id(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    module_id_ = id;
}

std::string StatusTracker::to_json() const {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json j;
    j["active_mode"] = mode_name(mode_);
    j["task_active"] = task_active_;
    j["stereo_camera"] = {
        {"connected", sc_connected_},
        {"capturing", sc_capturing_}
    };
    j["ai_dealer"] = {
        {"connected", ai_connected_},
        {"mode", ai_mode_}
    };
    j["database_size_gb"] = db_size_gb_;
    j["uploading"] = uploading_;
    j["module_id"] = module_id_;
    return j.dump();
}

} // namespace ecids_core
