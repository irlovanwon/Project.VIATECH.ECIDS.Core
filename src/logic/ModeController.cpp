/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: ModeController implementation
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented
 */

#include "ecids_core/logic/ModeController.h"
#include "ecids_core/common/Logger.h"
#include "ecids_core/data/StatusTracker.h"

namespace ecids_core {

ModeController& ModeController::instance() {
    static ModeController inst;
    return inst;
}

Mode ModeController::active_mode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return mode_;
}

bool ModeController::is_uploading() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return uploading_;
}

ResponseCode ModeController::set_mode(Mode new_mode) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (new_mode == Mode::Uploading) {
        Logger::warn("Use set_uploading() for Uploading mode");
        return ResponseCode::InvalidParam;
    }

    if (mode_ == new_mode) {
        Logger::info(std::string("ModeController: already in ") + mode_name(mode_));
        return ResponseCode::AlreadyInit;
    }

    // Allow free switching between exclusive modes.
    // Mutual exclusion is inherent — only one mode stored in mode_.
    // Task-active check is enforced by the API handler layer.

    Mode old = mode_;
    mode_ = new_mode;
    StatusTracker::instance().set_mode(new_mode);

    Logger::info(std::string("ModeController: ") + mode_name(old) + " -> " + mode_name(new_mode));

    if (callback_) {
        callback_(old, new_mode);
    }

    return ResponseCode::Success;
}

void ModeController::set_uploading(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    uploading_ = enabled;
    StatusTracker::instance().set_uploading(enabled);
    Logger::info(std::string("ModeController: uploading ") + (enabled ? "ON" : "OFF"));
}

void ModeController::on_mode_change(ModeChangeCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(cb);
}

} // namespace ecids_core
