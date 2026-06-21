/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Runtime status tracking
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented
 */

#ifndef ECIDS_CORE_DATA_STATUSTRACKER_H
#define ECIDS_CORE_DATA_STATUSTRACKER_H

#pragma once

#include "ecids_core/common/Types.h"
#include <string>
#include <mutex>
#include <atomic>

namespace ecids_core {

class StatusTracker {
public:
    static StatusTracker& instance();

    Mode active_mode() const;
    void set_mode(Mode m);

    bool task_active() const;
    void set_task_active(bool active);

    bool stereo_camera_connected() const;
    void set_stereo_camera_connected(bool v);

    bool stereo_camera_capturing() const;
    void set_stereo_camera_capturing(bool v);

    bool ai_dealer_connected() const;
    void set_ai_dealer_connected(bool v);

    std::string ai_dealer_mode() const;
    void set_ai_dealer_mode(const std::string& mode);

    double database_size_gb() const;
    void set_database_size_gb(double gb);

    bool uploading() const;
    void set_uploading(bool v);

    std::string module_id() const;
    void set_module_id(const std::string& id);

    std::string to_json() const;

private:
    StatusTracker() = default;

    mutable std::mutex mutex_;
    Mode mode_ = Mode::None;
    bool task_active_ = false;
    bool sc_connected_ = false;
    bool sc_capturing_ = false;
    bool ai_connected_ = false;
    std::string ai_mode_ = "unknown";
    double db_size_gb_ = 0.0;
    bool uploading_ = false;
    std::string module_id_ = "ECIDS-Core-01";
};

} // namespace ecids_core

#endif // ECIDS_CORE_DATA_STATUSTRACKER_H
