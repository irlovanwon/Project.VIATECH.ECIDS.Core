/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: PreprocessModule implementation
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented full pipeline
 */

#include "ecids_core/preprocess/PreprocessModule.h"
#include "ecids_core/data/DataBuffer.h"
#include "ecids_core/api2/DetectionDealer.h"
#include "ecids_core/database/RecordManager.h"
#include "ecids_core/common/Logger.h"
#include "ecids_core/common/Timestamp.h"

#include <nlohmann/json.hpp>
#include <opencv2/imgcodecs.hpp>
#include <filesystem>
#include <algorithm>
#include <unordered_map>
#include <fstream>
#include <unistd.h>

namespace ecids_core {

namespace fs = std::filesystem;
using json = nlohmann::json;

PreprocessModule::PreprocessModule() {}

PreprocessModule::~PreprocessModule() {
    stop();
}

void PreprocessModule::init(DataBuffer* buffer, DetectionDealer* dealer,
                            RecordManager* record_mgr, AIMode ai_mode) {
    buffer_ = buffer;
    dealer_ = dealer;
    record_mgr_ = record_mgr;
    ai_mode_ = ai_mode;

    if (dealer_) {
        dealer_->set_result_callback([this](const DetectionResponse& resp) {
            this->on_ai_result_(resp);
        });
    }
}

void PreprocessModule::set_result_callback(ResultCallback cb) {
    callback_ = std::move(cb);
}

void PreprocessModule::start_inspection(const std::string& record_path) {
    active_record_path_ = record_path;
    ai_test_mode_ = false;
    running_ = true;
    thread_ = std::thread(&PreprocessModule::inspection_loop_, this);
    Logger::info("PreprocessModule: inspection started");
}

void PreprocessModule::stop_inspection() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
    active_record_path_.clear();
    Logger::info("PreprocessModule: inspection stopped");
}

void PreprocessModule::start_ai_test(const std::string& test_data_path) {
    test_data_path_ = test_data_path;
    ai_test_mode_ = true;
    running_ = true;
    thread_ = std::thread(&PreprocessModule::ai_test_loop_, this);
    Logger::info("PreprocessModule: AI test started — " + test_data_path);
}

void PreprocessModule::stop_ai_test() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
    ai_test_mode_ = false;
    Logger::info("PreprocessModule: AI test stopped");
}

void PreprocessModule::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void PreprocessModule::inspection_loop_() {
    while (running_) {
        DataBundle left, right;

        bool got_pair = false;
        {
            auto channels = buffer_->channel_names();
            for (auto& ch : channels) {
                if (buffer_->get_latest(ch, left)) {
                    if (left.header.pair_id > 0 &&
                        buffer_->try_get_stereo_pair(left.header.pair_id, left, right)) {
                        got_pair = true;
                        break;
                    }
                }
            }
        }

        if (!got_pair) {
            usleep(50000);
            continue;
        }

        process_pair_(left, right);
    }
}

void PreprocessModule::ai_test_loop_() {
    if (!fs::exists(test_data_path_)) {
        Logger::error("PreprocessModule: test data path not found: " + test_data_path_);
        return;
    }

    std::vector<std::string> files;
    for (auto& entry : fs::directory_iterator(test_data_path_)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp") {
            files.push_back(entry.path().string());
        }
    }

    std::sort(files.begin(), files.end());
    Logger::info("PreprocessModule: found " + std::to_string(files.size()) + " images");

    for (const auto& filepath : files) {
        if (!running_) break;
        process_file_(filepath);
        usleep(500000);
    }

    Logger::info("PreprocessModule: AI test image iteration complete");
}

void PreprocessModule::process_pair_(const DataBundle& left, const DataBundle& right) {
    std::string ts = Timestamp::now_string();

    if (ai_mode_ != AIMode::Binary && !active_record_path_.empty() && record_mgr_) {
        std::string lfile = record_mgr_->save_image(
            active_record_path_, ts, "L", left.data->data(), left.data->size());
        std::string rfile = record_mgr_->save_image(
            active_record_path_, ts, "R", right.data->data(), right.data->size());

        std::vector<std::string> uris = {
            active_record_path_ + "/images/" + lfile,
            active_record_path_ + "/images/" + rfile
        };
        std::vector<std::string> fns = {lfile, rfile};

        if (dealer_) {
            dealer_->send_file_request(ts, uris, fns);
        }

        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_[ts] = {ts, left, right};
        }
    } else if (ai_mode_ == AIMode::Binary && dealer_) {
        dealer_->send_binary_request(ts,
            left.data->data(), left.data->size(),
            right.data->data(), right.data->size());

        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_[ts] = {ts, left, right};
        }
    }
}

void PreprocessModule::process_file_(const std::string& filepath) {
    std::vector<uint8_t> file_data;
    std::ifstream ifs(filepath, std::ios::binary);
    if (!ifs) return;
    ifs.seekg(0, std::ios::end);
    size_t sz = ifs.tellg();
    ifs.seekg(0);
    file_data.resize(sz);
    ifs.read(reinterpret_cast<char*>(file_data.data()), sz);

    std::string ts = Timestamp::now_string();
    std::string filename = fs::path(filepath).filename().string();

    if (dealer_) {
        dealer_->send_file_request(ts, {filepath}, {filename});
    }

    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        DataBundle bundle;
        bundle.header.channel = "ai_test";
        bundle.data = std::make_shared<std::vector<uint8_t>>(std::move(file_data));
        pending_[ts] = {ts, bundle, DataBundle{}};
    }
}

void PreprocessModule::on_ai_result_(const DetectionResponse& response) {
    DataBundle left, right;
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        auto it = pending_.find(response.transaction_id);
        if (it != pending_.end()) {
            left = it->second.left;
            right = it->second.right;
            pending_.erase(it);
        }
    }

    if (callback_) {
        callback_(response.transaction_id, response, left, right);
    }
}

} // namespace ecids_core
