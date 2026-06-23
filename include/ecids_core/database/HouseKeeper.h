/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Automatic house-keeping by size threshold
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented background house-keeping thread
 */

#ifndef ECIDS_CORE_DATABASE_HOUSEKEEPER_H
#define ECIDS_CORE_DATABASE_HOUSEKEEPER_H

#pragma once

#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace ecids_core {

class HouseKeeper {
public:
    HouseKeeper();
    ~HouseKeeper();

    void start(const std::string& root_path, double max_size_gb,
               int check_interval_sec = 60);
    void stop();

    using SizeProvider = std::function<double()>;
    void set_size_provider(SizeProvider provider);

private:
    void run_();
    void do_housekeep_();

    std::string root_path_;
    double max_size_gb_ = 50.0;
    int check_interval_sec_ = 60;
    std::atomic<bool> running_{false};
    std::thread thread_;
    std::condition_variable cv_;
    std::mutex mutex_;
    SizeProvider size_provider_;
};

} // namespace ecids_core

#endif // ECIDS_CORE_DATABASE_HOUSEKEEPER_H
