/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Mode routing and mutual exclusion (A-G)
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented mode state machine with mutual exclusion
 */

#ifndef ECIDS_CORE_LOGIC_MODECONTROLLER_H
#define ECIDS_CORE_LOGIC_MODECONTROLLER_H

#pragma once

#include "ecids_core/common/Types.h"
#include "ecids_core/common/Response.h"
#include <functional>
#include <mutex>

namespace ecids_core {

class ModeController {
public:
    static ModeController& instance();

    Mode active_mode() const;
    bool is_uploading() const;

    ResponseCode set_mode(Mode new_mode);
    void set_uploading(bool enabled);

    using ModeChangeCallback = std::function<void(Mode old_mode, Mode new_mode)>;
    void on_mode_change(ModeChangeCallback cb);

private:
    ModeController() = default;

    mutable std::mutex mutex_;
    Mode mode_ = Mode::None;
    bool uploading_ = false;
    ModeChangeCallback callback_;
};

} // namespace ecids_core

#endif // ECIDS_CORE_LOGIC_MODECONTROLLER_H
