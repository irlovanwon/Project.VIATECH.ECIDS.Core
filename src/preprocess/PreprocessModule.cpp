/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: PreprocessModule implementation
 * Date: 2026-06-18
 * Modification: 2026-06-24 Added FPS throttling for installation phase (2 FPS)
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
#include <chrono>

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
    pair_index_ = 0;
    sub_task_ = InspectionSubTask::Installation;
    running_ = true;
    thread_ = std::thread(&PreprocessModule::inspection_loop_, this);
    Logger::info("PreprocessModule: inspection started");
}

void PreprocessModule::stop_inspection() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
    active_record_path_.clear();
    sub_task_ = InspectionSubTask::None;
    Logger::info("PreprocessModule: inspection stopped");
}

void PreprocessModule::set_sub_task(InspectionSubTask st) {
    sub_task_ = st;
    Logger::info(std::string("PreprocessModule: sub-task changed to ") + subtask_name(st));
}

std::string PreprocessModule::current_subfolder_() const {
    switch (sub_task_.load()) {
        case InspectionSubTask::Installation:  return "installation";
        case InspectionSubTask::GapInspection: return "inspection";
        case InspectionSubTask::Marking:       return "marking";
        default:                               return "inspection";
    }
}

void PreprocessModule::start_ai_test(const std::string& test_data_path) {
    test_data_path_ = test_data_path;
    ai_test_mode_ = true;
    pair_index_ = 0;
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

static double calculate_working_distance(const uint8_t* depth_data, size_t depth_size) {
    if (!depth_data || depth_size < sizeof(float)) return 0.0;
    const float* floats = reinterpret_cast<const float*>(depth_data);
    size_t count = depth_size / sizeof(float);
    if (count == 0) return 0.0;

    size_t w = 1920;
    size_t h = count / w;
    if (h == 0 || w * h != count) return 0.0;

    size_t cy_start = h * 3 / 10;
    size_t cy_end = h * 7 / 10;
    size_t cx_start = w * 3 / 10;
    size_t cx_end = w * 7 / 10;

    double sum = 0.0;
    size_t valid = 0;
    for (size_t y = cy_start; y < cy_end; ++y) {
        for (size_t x = cx_start; x < cx_end; ++x) {
            float d = floats[y * w + x];
            if (d > 100.0 && d < 5000.0) {
                sum += d;
                ++valid;
            }
        }
    }
    if (valid == 0) return 0.0;
    return sum / valid;
}

void PreprocessModule::inspection_loop_() {
    auto last_ai_send = std::chrono::steady_clock::now();

    while (running_) {
        DataBundle frame;

        if (!buffer_->dequeue_inspection(frame, 200)) {
            continue;
        }

        if (frame.data->empty()) continue;
        DataBundle left = frame;
        DataBundle right;

        auto sub = sub_task_.load();

        if (sub == InspectionSubTask::Installation) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_ai_send).count();
            if (elapsed_ms < 500) {
                continue;
            }
            last_ai_send = now;
        }

        double working_dist = 0.0;

        if (sub == InspectionSubTask::Installation) {
            DataBundle depth;
            if (buffer_->dequeue_depth(depth, 0) && !depth.data->empty()) {
                working_dist = calculate_working_distance(depth.data->data(), depth.data->size());
            }
        }

        process_pair_(left, right, working_dist);
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

static std::vector<uint8_t> encode_jpeg_(const uint8_t* raw, size_t sz, int quality) {
    if (sz == 0) return {};
    if (sz % 4 == 0 && sz >= 1920 * 1200 * 4) {
        int w = 1920, h = static_cast<int>(sz / (1920 * 4));
        if (h == 0) h = 1;
        if (static_cast<size_t>(w) * h * 4 != sz) {
            w = static_cast<int>(sz / (1200 * 4));
            h = (w > 0) ? 1200 : 1;
        }
        if (static_cast<size_t>(w) * h * 4 == sz) {
            cv::Mat img(h, w, CV_8UC4, const_cast<uint8_t*>(raw));
            std::vector<uint8_t> encoded;
            std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, quality};
            cv::imencode(".jpg", img, encoded, params);
            if (!encoded.empty()) return encoded;
        }
    }
    return std::vector<uint8_t>(raw, raw + sz);
}

void PreprocessModule::process_pair_(const DataBundle& left, const DataBundle& right, double working_distance_mm) {
    if (left.data->empty()) return;

    int idx = pair_index_.fetch_add(1);
    std::string ts = Timestamp::now_string();
    std::string subfolder = current_subfolder_();

    std::vector<uint8_t> l_enc = encode_jpeg_(left.data->data(), left.data->size(), 95);

    bool is_gap = (sub_task_.load() == InspectionSubTask::GapInspection ||
                   sub_task_.load() == InspectionSubTask::Marking);

    if (ai_mode_ == AIMode::Binary && dealer_) {
        dealer_->send_binary_request(ts,
            l_enc.data(), l_enc.size(),
            right.data->empty() ? nullptr : right.data->data(),
            right.data->size());
    } else if (dealer_) {
        if (!active_record_path_.empty() && record_mgr_) {
            std::string lfile = record_mgr_->save_image(
                active_record_path_, subfolder, "L", idx,
                l_enc.data(), l_enc.size());
            std::vector<std::string> uris = {active_record_path_ + "/" + subfolder + "/" + lfile};
            std::vector<std::string> fns = {lfile};

            if (!right.data->empty()) {
                std::vector<uint8_t> r_enc = encode_jpeg_(right.data->data(), right.data->size(), 95);
                std::string rfile = record_mgr_->save_image(
                    active_record_path_, subfolder, "R", idx,
                    r_enc.data(), r_enc.size());
                uris.push_back(active_record_path_ + "/" + subfolder + "/" + rfile);
                fns.push_back(rfile);
            }
            dealer_->send_file_request(ts, uris, fns);
        }
    }

    if (is_gap && !active_record_path_.empty() && record_mgr_) {
        record_mgr_->save_image(
            active_record_path_, subfolder, "L", idx,
            l_enc.data(), l_enc.size());
    }

    DataBundle left_store;
    if (!is_gap) {
        left_store.data = std::make_shared<std::vector<uint8_t>>(std::move(l_enc));
    }

    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        if (pending_.size() > 200) pending_.clear();
        pending_[ts] = {ts, left_store, DataBundle{}, idx, sub_task_.load(), working_distance_mm};
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

    int idx = pair_index_.fetch_add(1);
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
            pending_[ts] = {ts, bundle, DataBundle{}, idx, InspectionSubTask::None, 0.0};
    }
}

void PreprocessModule::on_ai_result_(const DetectionResponse& response) {
    DataBundle left, right;
    int pidx = 0;
    InspectionSubTask sub = InspectionSubTask::None;
    double working_dist = 0.0;

    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        auto it = pending_.find(response.transaction_id);
        if (it != pending_.end()) {
            left = it->second.left;
            right = it->second.right;
            pidx = it->second.pair_index;
            sub = it->second.sub_task;
            working_dist = it->second.working_distance_mm;
            pending_.erase(it);
        }
    }

    if (!active_record_path_.empty() && record_mgr_) {
        std::string subfolder = (sub == InspectionSubTask::Installation) ? "installation" : "inspection";
        if (ai_test_mode_.load()) subfolder = "";

        if (sub == InspectionSubTask::Installation && !left.data->empty()) {
            record_mgr_->save_image(
                active_record_path_, subfolder, "L", pidx,
                left.data->data(), left.data->size());
        }

        std::string ai_json_str;
        {
            json j;
            j["transaction_id"] = response.transaction_id;
            j["dealer_id"] = response.dealer_id;
            json dets = json::array();
            for (const auto& d : response.results) {
                dets.push_back({
                    {"label_id", d.label_id},
                    {"confidence", d.confidence},
                    {"file_name", d.file_name}
                });
            }
            j["results"] = dets;
            ai_json_str = j.dump();
        }

        if (subfolder.empty()) {
            record_mgr_->save_ai_result(active_record_path_, "", "L", pidx, ai_json_str);
        } else {
            record_mgr_->save_ai_result(active_record_path_, subfolder, "L", pidx, ai_json_str);
        }
    }

    if (callback_) {
        callback_(response.transaction_id, response, left, right, pidx, sub, working_dist);
    }
}

} // namespace ecids_core
