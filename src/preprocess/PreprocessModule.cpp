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
    if (thread_.joinable()) thread_.join();
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

void PreprocessModule::start_ai_test(const std::string& test_data_path, const std::string& record_path) {
    if (thread_.joinable()) thread_.join();
    test_data_path_ = test_data_path;
    active_record_path_ = record_path;
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

void PreprocessModule::clear_pending() {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    size_t count = pending_.size();
    pending_.clear();
    Logger::info("PreprocessModule: cleared " + std::to_string(count) + " pending requests on AIVD reconnect");
}

static double calculate_working_distance(const uint8_t* depth_data, size_t depth_size) {
    if (!depth_data || depth_size < sizeof(float)) return 0.0;
    const float* floats = reinterpret_cast<const float*>(depth_data);
    size_t count = depth_size / sizeof(float);
    if (count == 0) return 0.0;

    size_t w = 1920;
    size_t h = count / w;
    if (h == 0 || w * h != count) {
        return 0.0;
    }

    size_t cy_start = h * 3 / 10;
    size_t cy_end = h * 7 / 10;
    size_t cx_start = w * 3 / 10;
    size_t cx_end = w * 7 / 10;

    double sum = 0.0;
    size_t valid = 0;
    for (size_t y = cy_start; y < cy_end; ++y) {
        for (size_t x = cx_start; x < cx_end; ++x) {
            float d = floats[y * w + x];
            if (std::isnan(d) || std::isinf(d) || d == 0.0f) continue;
            double d_mm = d * 1000.0;
            if (d_mm > 100.0 && d_mm < 5000.0) {
                sum += d_mm;
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

        DataBundle left, right;
        const auto& raw = *frame.data;
        if (raw.size() == 1920 * 2400 * 4) {
            size_t half = raw.size() / 2;
            left.data = std::make_shared<std::vector<uint8_t>>(raw.begin(), raw.begin() + half);
            right.data = std::make_shared<std::vector<uint8_t>>(raw.begin() + half, raw.end());
            left.header = frame.header;
            right.header = frame.header;
        } else {
            left = frame;
        }

        auto sub = sub_task_.load();

        if (sub == InspectionSubTask::Installation) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_ai_send).count();
            int min_interval_ms = static_cast<int>(1000.0 / installation_fps_);
            if (elapsed_ms < min_interval_ms) {
                continue;
            }
            last_ai_send = now;
        }

        double working_dist = 0.0;

        if (sub == InspectionSubTask::Installation) {
            DataBundle depth;
            bool got_depth = buffer_->dequeue_depth(depth, 0);
            if (got_depth && !depth.data->empty()) {
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

    // Scan all image files
    std::vector<std::string> files;
    for (auto& entry : fs::directory_iterator(test_data_path_)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp") {
            files.push_back(entry.path().string());
        }
    }

    // Try to pair files by naming convention: <prefix>_<index>.ext
    // e.g. 1_5.bmp (left) pairs with 2_5.bmp (right)
    std::map<std::string, std::pair<std::string, std::string>> pairs;
    std::map<std::string, bool> used;
    std::vector<std::string> unpaired;

    for (const auto& f : files) {
        std::string stem = fs::path(f).stem().string();
        size_t us = stem.find('_');
        if (us != std::string::npos && us > 0) {
            std::string prefix = stem.substr(0, us);
            std::string suffix = stem.substr(us + 1);
            if (prefix == "1" || prefix == "L" || prefix == "l" || prefix == "left") {
                if (pairs.find(suffix) == pairs.end())
                    pairs[suffix] = {"", ""};
                pairs[suffix].first = f;
                used[f] = true;
            } else if (prefix == "2" || prefix == "R" || prefix == "r" || prefix == "right") {
                if (pairs.find(suffix) == pairs.end())
                    pairs[suffix] = {"", ""};
                pairs[suffix].second = f;
                used[f] = true;
            }
        }
    }

    // Files not matching the pairing convention
    for (const auto& f : files) {
        if (!used.count(f)) unpaired.push_back(f);
    }

    // Build sorted pair list
    struct PairEntry {
        std::string left;
        std::string right;
    };
    std::vector<PairEntry> entries;

    for (const auto& [key, lr] : pairs) {
        if (!lr.first.empty() || !lr.second.empty()) {
            entries.push_back({lr.first, lr.second});
        }
    }
    std::sort(entries.begin(), entries.end(), [](const PairEntry& a, const PairEntry& b) {
        std::string ak = fs::path(a.left.empty() ? a.right : a.left).stem().string();
        std::string bk = fs::path(b.left.empty() ? b.right : b.left).stem().string();
        return ak < bk;
    });
    for (const auto& f : unpaired) {
        entries.push_back({f, ""});
    }
    std::sort(unpaired.begin(), unpaired.end());

    Logger::info("PreprocessModule: found " + std::to_string(entries.size()) + " image pairs");

    int processed = 0;
    for (const auto& e : entries) {
        if (!running_) break;
        process_pair_file_(e.left, e.right);
        usleep(500000);
        ++processed;
    }

    Logger::info("PreprocessModule: AI test complete — processed " + std::to_string(processed) + " pairs");

    if (completion_callback_) {
        completion_callback_();
    }
}

void PreprocessModule::process_pair_file_(const std::string& left_path,
                                           const std::string& right_path) {
    int idx = pair_index_.fetch_add(1);
    std::string ts = Timestamp::now_string();

    // Read left image
    std::vector<uint8_t> l_jpeg;
    if (!left_path.empty()) {
        cv::Mat img = cv::imread(left_path);
        if (img.empty()) {
            Logger::error("PreprocessModule: failed to read image: " + left_path);
            return;
        }
        cv::imencode(".jpg", img, l_jpeg, {cv::IMWRITE_JPEG_QUALITY, 95});
    }

    // Read right image
    std::vector<uint8_t> r_jpeg;
    if (!right_path.empty()) {
        cv::Mat img = cv::imread(right_path);
        if (!img.empty()) {
            cv::imencode(".jpg", img, r_jpeg, {cv::IMWRITE_JPEG_QUALITY, 95});
        }
    }

    // Send to AI dealer
    if (ai_mode_ == AIMode::Binary && dealer_) {
        dealer_->send_binary_request(ts,
            l_jpeg.empty() ? nullptr : l_jpeg.data(),
            l_jpeg.size(),
            r_jpeg.empty() ? nullptr : r_jpeg.data(),
            r_jpeg.size());
    } else if (dealer_) {
        std::vector<std::string> paths, names;
        if (!left_path.empty()) {
            paths.push_back(left_path);
            names.push_back(fs::path(left_path).filename().string());
        }
        if (!right_path.empty()) {
            paths.push_back(right_path);
            names.push_back(fs::path(right_path).filename().string());
        }
        dealer_->send_file_request(ts, paths, names);
    }

    // Store in pending_
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        DataBundle left_bundle, right_bundle;
        if (!l_jpeg.empty()) {
            left_bundle.header.channel = "ai_test";
            left_bundle.data = std::make_shared<std::vector<uint8_t>>(std::move(l_jpeg));
        }
        if (!r_jpeg.empty()) {
            right_bundle.header.channel = "ai_test";
            right_bundle.data = std::make_shared<std::vector<uint8_t>>(std::move(r_jpeg));
        }
        pending_[ts] = {ts, left_bundle, right_bundle, idx, InspectionSubTask::None, 0.0};
    }
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
    std::vector<uint8_t> r_enc;
    if (!right.data->empty()) {
        r_enc = encode_jpeg_(right.data->data(), right.data->size(), 95);
    }

    bool is_gap = (sub_task_.load() == InspectionSubTask::GapInspection ||
                   sub_task_.load() == InspectionSubTask::Marking);

    if (ai_mode_ == AIMode::Binary && dealer_) {
        dealer_->send_binary_request(ts,
            l_enc.data(), l_enc.size(),
            r_enc.empty() ? nullptr : r_enc.data(),
            r_enc.size());
    } else if (dealer_) {
        if (!active_record_path_.empty() && record_mgr_) {
            std::string lfile = record_mgr_->save_image(
                active_record_path_, subfolder, "L", idx,
                l_enc.data(), l_enc.size());
            std::vector<std::string> uris = {active_record_path_ + "/" + subfolder + "/" + lfile};
            std::vector<std::string> fns = {lfile};

            if (!r_enc.empty()) {
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
        if (!r_enc.empty()) {
            record_mgr_->save_image(
                active_record_path_, subfolder, "R", idx,
                r_enc.data(), r_enc.size());
        }
    }

    DataBundle left_store, right_store;
    if (!is_gap) {
        left_store.data = std::make_shared<std::vector<uint8_t>>(std::move(l_enc));
        if (!r_enc.empty()) {
            right_store.data = std::make_shared<std::vector<uint8_t>>(std::move(r_enc));
        }
    }

    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        if (pending_.size() > 200) pending_.clear();
        pending_[ts] = {ts, left_store, right_store, idx, sub_task_.load(), working_distance_mm};
    }
}

void PreprocessModule::process_file_(const std::string& filepath) {
    int idx = pair_index_.fetch_add(1);
    std::string ts = Timestamp::now_string();
    std::string filename = fs::path(filepath).filename().string();

    cv::Mat img = cv::imread(filepath);
    if (img.empty()) {
        Logger::error("PreprocessModule: failed to read image: " + filepath);
        return;
    }
    std::vector<uint8_t> jpeg;
    cv::imencode(".jpg", img, jpeg, {cv::IMWRITE_JPEG_QUALITY, 95});

    if (ai_mode_ == AIMode::Binary && dealer_) {
        dealer_->send_binary_request(ts,
            jpeg.data(), jpeg.size(),
            nullptr, 0);
    } else if (dealer_) {
        dealer_->send_file_request(ts, {filepath}, {filename});
    }

    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        DataBundle bundle;
        bundle.header.channel = "ai_test";
        bundle.data = std::make_shared<std::vector<uint8_t>>(std::move(jpeg));
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
        } else {
            if (!pending_.empty()) {
                auto oldest = std::min_element(pending_.begin(), pending_.end(),
                    [](const auto& a, const auto& b) { return a.first < b.first; });
                // Skip stale entries (>30s old, likely orphaned by restart)
                auto age = std::chrono::steady_clock::now() - oldest->second.created_at;
                if (age > std::chrono::seconds(30)) {
                    Logger::warn("PreprocessModule: skipping stale pending entry (age=" +
                                 std::to_string(std::chrono::duration_cast<std::chrono::seconds>(age).count()) +
                                 "s), likely orphaned by restart");
                    pending_.erase(oldest);
                    return;
                }
                left = oldest->second.left;
                right = oldest->second.right;
                pidx = oldest->second.pair_index;
                sub = oldest->second.sub_task;
                working_dist = oldest->second.working_distance_mm;
                pending_.erase(oldest);
            }
        }
    }

    if (!active_record_path_.empty() && record_mgr_) {
        std::string subfolder = (sub == InspectionSubTask::Installation) ? "installation" : "inspection";
        if (ai_test_mode_.load()) subfolder = "";

        if (!left.data->empty()) {
            record_mgr_->save_image(
                active_record_path_, subfolder, "L", pidx,
                left.data->data(), left.data->size());
        }
        if (!right.data->empty()) {
            record_mgr_->save_image(
                active_record_path_, subfolder, "R", pidx,
                right.data->data(), right.data->size());
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
            record_mgr_->save_ai_result(active_record_path_, "", "R", pidx, ai_json_str);
        } else {
            record_mgr_->save_ai_result(active_record_path_, subfolder, "L", pidx, ai_json_str);
            record_mgr_->save_ai_result(active_record_path_, subfolder, "R", pidx, ai_json_str);
        }
    }

    if (callback_) {
        callback_(response.transaction_id, response, left, right, pidx, sub, working_dist);
    }
}

} // namespace ecids_core
