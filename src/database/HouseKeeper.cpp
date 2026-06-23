/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: HouseKeeper implementation
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented
 */

#include "ecids_core/database/HouseKeeper.h"
#include "ecids_core/database/RecordManager.h"
#include "ecids_core/common/Logger.h"

#include <filesystem>
#include <algorithm>
#include <chrono>

namespace ecids_core {

namespace fs = std::filesystem;

HouseKeeper::HouseKeeper() = default;

HouseKeeper::~HouseKeeper() {
    stop();
}

void HouseKeeper::start(const std::string& root_path, double max_size_gb,
                         int check_interval_sec) {
    root_path_ = root_path;
    max_size_gb_ = max_size_gb;
    check_interval_sec_ = check_interval_sec;
    running_ = true;
    thread_ = std::thread(&HouseKeeper::run_, this);
    Logger::info("HouseKeeper: started — root=" + root_path
                 + " max=" + std::to_string(max_size_gb) + "GB");
}

void HouseKeeper::stop() {
    if (!running_) return;
    running_ = false;
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
    Logger::info("HouseKeeper: stopped");
}

void HouseKeeper::set_size_provider(SizeProvider provider) {
    std::lock_guard<std::mutex> lock(mutex_);
    size_provider_ = std::move(provider);
}

void HouseKeeper::run_() {
    while (running_) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, std::chrono::seconds(check_interval_sec_),
                         [this] { return !running_; });
        }
        if (!running_) break;
        try {
            do_housekeep_();
        } catch (const std::exception& e) {
            Logger::error(std::string("HouseKeeper: ") + e.what());
        }
    }
}

void HouseKeeper::do_housekeep_() {
    double current_size;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (size_provider_) {
            current_size = size_provider_();
        } else {
            current_size = RecordManager::calculate_size_gb(root_path_);
        }
    }

    if (current_size <= max_size_gb_) return;

    Logger::info("HouseKeeper: size=" + std::to_string(current_size)
                 + "GB threshold=" + std::to_string(max_size_gb_) + "GB — cleaning");

    if (!fs::exists(root_path_)) return;

    std::vector<fs::path> year_dirs;
    std::error_code ec;
    for (auto& entry : fs::directory_iterator(root_path_, ec)) {
        if (entry.is_directory() && entry.path().filename().string().size() == 4) {
            year_dirs.push_back(entry.path());
        }
    }

    std::sort(year_dirs.begin(), year_dirs.end());

    while (current_size > max_size_gb_ && !year_dirs.empty()) {
        fs::path oldest = year_dirs.front();
        year_dirs.erase(year_dirs.begin());

        uintmax_t removed = 0;
        std::error_code rec;
        fs::remove_all(oldest, rec);

        Logger::info("HouseKeeper: removed " + oldest.string());

        if (size_provider_) {
            current_size = size_provider_();
        } else {
            current_size = RecordManager::calculate_size_gb(root_path_);
        }
    }
}

} // namespace ecids_core
