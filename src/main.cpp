/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: ECIDS Core — entry point, wires all modules
 * Date: 2026-06-18
 * Modification: 2026-06-24 Added live image forwarding, detection annotation, station_id validation, installation status
 */

#include "ecids_core/common/Logger.h"
#include "ecids_core/common/Types.h"
#include "ecids_core/common/Response.h"
#include "ecids_core/data/ConfigManager.h"
#include "ecids_core/data/DataBuffer.h"
#include "ecids_core/data/StatusTracker.h"
#include "ecids_core/logic/ModeController.h"
#include "ecids_core/logic/TaskManager.h"
#include "ecids_core/database/DatabaseManager.h"
#include "ecids_core/api1/DataSubscriber.h"
#include "ecids_core/api1/StereoCameraClient.h"
#include "ecids_core/api2/DetectionDealer.h"
#include "ecids_core/api2/AIAdminClient.h"
#include "ecids_core/api3/ClientServer.h"
#include "ecids_core/api3/DataPublisher.h"
#include "ecids_core/api3/WSSServer.h"
#include "ecids_core/preprocess/PreprocessModule.h"
#include "ecids_core/postprocess/PostprocessModule.h"

#include <nlohmann/json.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <csignal>
#include <cstdlib>
#include <cstdio>
#include <atomic>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace fs = std::filesystem;

using json = nlohmann::json;
using namespace ecids_core;

static std::atomic<bool> g_running{true};
static std::atomic<bool> g_processing_paused{false};

static void signal_handler(int sig) {
    (void)sig;
    g_running = false;
}

static std::string make_response(int code, const std::string& message,
                                 const std::string& data_json = "{}") {
    json j;
    j["code"] = code;
    j["message"] = message;
    j["data"] = json::parse(data_json, nullptr, false);
    if (j["data"].is_discarded()) j["data"] = data_json;
    return j.dump();
}

static json scan_inspection_records(const std::string& db_root) {
    json records = json::array();
    if (!fs::exists(db_root)) return records;

    std::vector<fs::path> record_dirs;
    try {
        for (auto& yr : fs::directory_iterator(db_root)) {
            if (!yr.is_directory()) continue;
            for (auto& mo : fs::directory_iterator(yr.path())) {
                if (!mo.is_directory()) continue;
                for (auto& dy : fs::directory_iterator(mo.path())) {
                    if (!dy.is_directory()) continue;
                    for (auto& rec : fs::directory_iterator(dy.path())) {
                        if (!rec.is_directory()) continue;
                        std::string name = rec.path().filename().string();
                        if (name.rfind("(Inspection)", 0) != 0) continue;
                        record_dirs.push_back(rec.path());
                    }
                }
            }
        }
    } catch (...) {}

    for (const auto& rdir : record_dirs) {
        std::string dirname = rdir.filename().string();
        std::string after = dirname.substr(std::string("(Inspection)").size());

        std::vector<std::string> toks;
        {
            size_t start = 0;
            for (size_t i = 0; i <= after.size(); ++i) {
                if (i == after.size() || after[i] == '-') {
                    toks.push_back(after.substr(start, i - start));
                    start = i + 1;
                }
            }
        }

        std::string date_str, time_str, station, escalator, task;
        if (toks.size() >= 3) {
            date_str = toks[0] + "-" + toks[1] + "-" + toks[2];
        }
        if (toks.size() >= 6) {
            time_str = toks[3] + ":" + toks[4] + ":" + toks[5];
        }
        if (toks.size() >= 7) station = toks[6];
        if (toks.size() >= 8) escalator = toks[7];
        if (toks.size() >= 9) task = toks[8];

        json rec;
        rec["id"] = dirname;
        std::string rel = rdir.string();
        if (rel.rfind(db_root, 0) == 0) rel = rel.substr(db_root.size());
        while (!rel.empty() && rel[0] == '/') rel = rel.substr(1);
        rec["path"] = rel;
        rec["inspectionDate"] = date_str + (time_str.empty() ? "" : " " + time_str);
        rec["stationId"] = station;
        rec["escalatorId"] = escalator;
        rec["taskId"] = task;

        std::string insp_dir = rdir.string() + "/inspection";
        double gap_sum = 0;
        int gap_count = 0, gaps_over5 = 0, defects = 0, step_count = 0;

        if (fs::exists(insp_dir)) {
            std::vector<fs::path> jfiles;
            for (auto& f : fs::directory_iterator(insp_dir)) {
                if (f.is_regular_file() && f.path().extension() == ".json") {
                    std::string fn = f.path().filename().string();
                    if (fn.rfind("Stereo_", 0) == 0) jfiles.push_back(f.path());
                }
            }
            step_count = static_cast<int>(jfiles.size());
            for (const auto& jf : jfiles) {
                try {
                    std::ifstream ifs(jf);
                    json data;
                    ifs >> data;
                    double gap = data.value("gap_distance_mm", 0.0);
                    if (gap > 0) {
                        gap_sum += gap;
                        ++gap_count;
                        if (gap > 5.0) ++gaps_over5;
                    }
                    if (data.contains("ai_detections") && data["ai_detections"].is_array()) {
                        for (const auto& d : data["ai_detections"]) {
                            if (d.value("confidence", 0.0) >= 0.5) ++defects;
                        }
                    }
                } catch (...) {}
            }
        }

        rec["stepCount"] = step_count;
        rec["aveGapDistance"] = gap_count > 0
            ? std::round((gap_sum / gap_count) * 10.0) / 10.0 : 0.0;
        rec["gapsOver5"] = gaps_over5;
        rec["defects"] = defects;
        records.push_back(rec);
    }

    std::sort(records.begin(), records.end(), [](const json& a, const json& b) {
        return a.value("inspectionDate", "") > b.value("inspectionDate", "");
    });
    return records;
}

static json read_record_details(const std::string& db_root,
                                const std::string& record_path) {
    json result;
    std::string full = db_root;
    if (!record_path.empty() && record_path[0] != '/') full += "/";
    full += record_path;

    if (!fs::exists(full)) {
        result["error"] = "Record not found";
        return result;
    }

    json steps = json::array();
    std::string insp_dir = full + "/inspection";

    if (fs::exists(insp_dir)) {
        std::vector<fs::path> jfiles;
        for (auto& f : fs::directory_iterator(insp_dir)) {
            if (f.is_regular_file() && f.path().extension() == ".json") {
                std::string fn = f.path().filename().string();
                if (fn.rfind("Stereo_", 0) == 0) jfiles.push_back(f.path());
            }
        }
        std::sort(jfiles.begin(), jfiles.end());

        for (const auto& jf : jfiles) {
            try {
                std::ifstream ifs(jf);
                json data;
                ifs >> data;

                json step;
                step["pairIndex"] = data.value("pair_index", 0);
                step["timestamp"] = data.value("timestamp", "");
                step["gapDistance"] = std::round(
                    data.value("gap_distance_mm", 0.0) * 10.0) / 10.0;
                step["workingDistance"] = std::round(
                    data.value("working_distance_mm", 0.0) * 10.0) / 10.0;

                std::string det = "Normal";
                if (data.contains("ai_detections") && data["ai_detections"].is_array()) {
                    for (const auto& d : data["ai_detections"]) {
                        if (d.value("confidence", 0.0) >= 0.5) {
                            det = (d.value("label_id", 0) == 1) ? "Warning" : "Defect";
                            break;
                        }
                    }
                }
                step["aiDetection"] = det;
                step["isFirstStep"] = (data.value("pair_index", -1) == 0);
                steps.push_back(step);
            } catch (...) {}
        }
    }

    result["steps"] = steps;
    result["stepCount"] = steps.size();
    return result;
}

int main(int argc, char* argv[]) {
    std::string config_path = "config/default.json";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--config" || arg == "-c") && i + 1 < argc) {
            config_path = argv[++i];
        }
    }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    Logger::info("=== ECIDS Core starting ===");

    auto& cfg = ConfigManager::instance();
    if (!cfg.load(config_path)) {
        Logger::error("Failed to load config: " + config_path);
        return 1;
    }

    StatusTracker::instance().set_module_id(cfg.get_string("module_id", "ECIDS-Core-01"));

    auto& db = DatabaseManager::instance();
    db.init(cfg.get_string("database.root_path", "database"),
            cfg.get_bool("database.enable_housekeep", true),
            cfg.get_double("database.max_size_gb", 50));

    DataBuffer buffer;
    buffer.set_inspection_capacity(cfg.get_int("preprocess.spsc_capacity", 100));

    StereoCameraClient sc_client;
    sc_client.init(cfg.get_string("api1b.stereo_camera_host", "127.0.0.1"),
                   cfg.get_int("api1b.stereo_camera_port", 9443),
                   cfg.get_int("api1b.timeout_ms", 5000));

    AIAdminClient ai_client;
    ai_client.init(cfg.get_string("api2b.ai_admin_host", "127.0.0.1"),
                   cfg.get_int("api2b.ai_admin_port", 8445),
                   cfg.get_int("api2b.timeout_ms", 5000));

    json cfg_json;
    {
        std::ifstream cfg_file(config_path);
        try { cfg_file >> cfg_json; } catch (...) {
            Logger::error("Failed to parse config JSON");
            return 1;
        }
    }

    DataPublisher publisher;
    if (cfg_json.contains("api3b") && cfg_json["api3b"].contains("endpoints")) {
        for (auto& [name, endpoint] : cfg_json["api3b"]["endpoints"].items()) {
            publisher.add_channel(name, endpoint.get<std::string>(),
                                  cfg_json["api3b"].value("sndhwm", 10));
        }
    }
    publisher.start();

    WSSServer wss_server;

    DataSubscriber subscriber;
    if (cfg_json.contains("api1a") && cfg_json["api1a"].contains("channels")) {
        for (auto& [name, endpoint] : cfg_json["api1a"]["channels"].items()) {
            subscriber.add_channel(name, endpoint.get<std::string>());
        }
    }

    subscriber.set_callback([&](const std::string& channel, const DataBundle& bundle) {
        buffer.put(channel, bundle);

        if (channel == "stereo_image" || channel == "visual_geometric_2d") {
            buffer.put_stereo(bundle.header.pair_id, bundle.header.part, bundle);
            if (!g_processing_paused.load()) {
                buffer.enqueue_inspection(bundle);
            }

            if (bundle.header.part.empty() || bundle.header.part == "left") {
                static auto last_live_ts = std::chrono::steady_clock::now();
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - last_live_ts).count() >= 200) {
                    last_live_ts = now;
                    const auto& raw = *bundle.data;
                    if (!raw.empty() && raw.size() % 4 == 0) {
                        int w = 1920;
                        int h = static_cast<int>(raw.size() / (1920u * 4));
                        if (h > 0 && static_cast<size_t>(w) * h * 4 == raw.size()) {
                            cv::Mat img(h, w, CV_8UC4, const_cast<uint8_t*>(raw.data()));
                            std::vector<uint8_t> jpeg;
                            cv::imencode(".jpg", img, jpeg, {cv::IMWRITE_JPEG_QUALITY, 70});
                            if (!jpeg.empty()) {
                                wss_server.broadcast_binary("core/live_image",
                                    jpeg.data(), jpeg.size());
                            }
                        }
                    }
                }
            }
        }

        if (channel == "depth_map" || channel == "visual_geometric_2d") {
            buffer.enqueue_depth(bundle);
        }

        Mode mode = ModeController::instance().active_mode();
        if (mode == Mode::Data) {
            json hdr;
            hdr["channel"] = channel;
            hdr["ts_sec"] = bundle.header.ts_sec;
            hdr["ts_nsec"] = bundle.header.ts_nsec;
            (void)hdr;
        }
    });
    subscriber.start();

    DetectionDealer dealer;
    {
        std::string api2a_transport = cfg.get_string("api2a.transport", "ipc");
        std::string pub_endpoint = (api2a_transport == "tcp")
            ? cfg.get_string("api2a.pub_endpoint_remote", "tcp://localhost:5555")
            : cfg.get_string("api2a.pub_endpoint_local", "ipc:///tmp/ai_vision_dealer_detection");
        std::string sub_endpoint = (api2a_transport == "tcp")
            ? cfg.get_string("api2a.sub_endpoint_remote", "tcp://localhost:5556")
            : cfg.get_string("api2a.sub_endpoint_local", "ipc:///tmp/ai_vision_dealer_result");

        dealer.init(pub_endpoint, sub_endpoint,
                    cfg.get_string("api2a.identity", "ecids_core"),
                    cfg.get_int("api2a.sndhwm", 10),
                    cfg.get_int("api2a.rcvhwm", 10),
                    cfg.get_int("api2a.poll_timeout_ms", 100));
    }
    dealer.start();

    AIMode ai_mode = ai_mode_from_string(cfg.get_string("api2a.ai_mode", "file"));

    PostprocessModule postprocess;
    postprocess.init(cfg.get_double("postprocess.min_confidence", 0.5),
                     cfg.get_double("stereo_params.baseline_mm", 120),
                     cfg.get_double("stereo_params.focal_length_px", 0));
    postprocess.set_record_manager(&db.records());

    PreprocessModule preprocess;
    preprocess.init(&buffer, &dealer, &db.records(), ai_mode);
    preprocess.set_installation_fps(cfg.get_double("preprocess.installation_fps", 1.0));

    TaskManager task_mgr;

    preprocess.set_result_callback([&](const std::string& txn_id,
                                       const DetectionResponse& resp,
                                       const DataBundle& left,
                                       const DataBundle& right,
                                       int pair_index,
                                       InspectionSubTask sub_task,
                                       double working_distance_mm) {
        (void)txn_id;

        Mode mode = ModeController::instance().active_mode();
        InspectionResult result;

        if (mode == Mode::AITest) {
            result = postprocess.process_ai_test(resp, pair_index);
        } else {
            result = postprocess.process(resp,
                task_mgr.inspection().station_id,
                task_mgr.inspection().escalator_id,
                task_mgr.inspection().task_id,
                left.data->data(), left.data->size(),
                right.data->data(), right.data->size(),
                pair_index, sub_task, working_distance_mm);
        }

        json result_json;
        result_json["transaction_id"] = result.transaction_id;
        result_json["task_id"] = result.task_id;
        result_json["station_id"] = result.station_id;
        result_json["escalator_id"] = result.escalator_id;
        result_json["gap_distance_mm"] = result.gap_distance_mm;
        result_json["working_distance_mm"] = result.working_distance_mm;
        result_json["timestamp"] = result.timestamp;
        result_json["sub_task"] = subtask_name(sub_task);

        json dets = json::array();
        for (const auto& d : result.ai_detections) {
            json det = {{"label_id", d.label_id}, {"confidence", d.confidence}};
            if (!d.coordinates.empty()) {
                json coords = json::array();
                for (const auto& c : d.coordinates) {
                    coords.push_back({c.first, c.second});
                }
                det["coordinates"] = coords;
            }
            dets.push_back(det);
        }
        result_json["ai_detections"] = dets;
        result_json["abnormal"] = json::array();
        for (const auto& a : result.abnormal) {
            result_json["abnormal"].push_back({{"label_id", a.label_id}, {"confidence", a.confidence}});
        }

        if (sub_task == InspectionSubTask::Installation) {
            bool wd_ok = (result.working_distance_mm >= 300.0 &&
                          result.working_distance_mm <= 1000.0);
            bool task_ok = !result.task_id.empty();
            result_json["installation_ready"] = (wd_ok && task_ok);
            result_json["working_distance_ok"] = wd_ok;
            result_json["task_detected"] = task_ok;
        }

        publisher.publish_json("Visual2D", result_json.dump());
        wss_server.broadcast_json("core/result", result_json.dump());

        if (!left.data->empty()) {
            cv::Mat img = cv::imdecode(*left.data, cv::IMREAD_COLOR);
            if (!img.empty()) {
                for (const auto& d : result.ai_detections) {
                    if (d.coordinates.size() >= 2) {
                        auto& p1 = d.coordinates[0];
                        auto& p2 = d.coordinates[1];
                        cv::rectangle(img,
                            cv::Point(static_cast<int>(p1.first), static_cast<int>(p1.second)),
                            cv::Point(static_cast<int>(p2.first), static_cast<int>(p2.second)),
                            cv::Scalar(0, 255, 0), 2);
                        std::string label = d.label_id + " " +
                            std::to_string(d.confidence).substr(0, 4);
                        int baseline = 0;
                        auto ts = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.6, 2, &baseline);
                        int y_pos = std::max(0, static_cast<int>(p1.second) - 5);
                        cv::putText(img, label, cv::Point(static_cast<int>(p1.first), y_pos),
                            cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);
                    }
                }

                std::string wd_text = "WD: " +
                    std::to_string(result.working_distance_mm).substr(0, 6) + "mm";
                cv::putText(img, wd_text, cv::Point(10, 30),
                    cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 255), 2);

                if (sub_task == InspectionSubTask::Installation) {
                    bool wd_ok = (result.working_distance_mm >= 300.0 &&
                                  result.working_distance_mm <= 1000.0);
                    std::string status = wd_ok ? "WD OK" : "WD OUT OF RANGE";
                    cv::Scalar color = wd_ok ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255);
                    cv::putText(img, status, cv::Point(10, 60),
                        cv::FONT_HERSHEY_SIMPLEX, 0.7, color, 2);
                }

                std::vector<uint8_t> annotated;
                cv::imencode(".jpg", img, annotated, {cv::IMWRITE_JPEG_QUALITY, 80});
                if (!annotated.empty()) {
                    wss_server.broadcast_binary("core/result_image",
                        annotated.data(), annotated.size(), result_json.dump());
                }
            }
        }

        Logger::info("Result published: txn=" + result.transaction_id
                     + " gap=" + std::to_string(result.gap_distance_mm) + "mm"
                     + " wd=" + std::to_string(result.working_distance_mm) + "mm");
    });

    ClientServer client_server;
    client_server.set_forward_handler([&](const std::string& method,
                                          const std::string& path,
                                          const std::string& body) -> std::string {
        Logger::debug("API3a: " + method + " " + path);

        if (method == "GET" && path == "/CheckStatus") {
            std::string status = StatusTracker::instance().to_json();
            return make_response(0, "Status", status);
        }

        if (method == "GET" && path == "/GetHistory") {
            json records = scan_inspection_records(db.root_path());
            json data;
            data["records"] = records;
            data["count"] = records.size();
            return make_response(0, "Success", data.dump());
        }

        if (method == "POST" && path == "/GetRecord") {
            try {
                json req = json::parse(body);
                std::string rec_path = req.value("record_path", "");
                json details = read_record_details(db.root_path(), rec_path);
                return make_response(0, "Success", details.dump());
            } catch (const std::exception& e) {
                return make_response(1, e.what());
            }
        }

        if (method == "POST" && path == "/SetMode") {
            try {
                json req = json::parse(body);
                std::string mode_str = req.value("mode", "");
                Mode new_mode = mode_from_string(mode_str);

                if (new_mode == Mode::None) {
                    return make_response(4, "Invalid mode: " + mode_str);
                }

                if (StatusTracker::instance().task_active()) {
                    return make_response(6, "ModeConflict: task active, stop first");
                }

                auto rc = ModeController::instance().set_mode(new_mode);
                if (rc == ResponseCode::Success) {
                    json status_json;
                    status_json["active_mode"] = mode_str;
                    publisher.publish_json("Visual2D", status_json.dump());
                    wss_server.broadcast_json("core/status", status_json.dump());

                    return make_response(0, "Mode set to " + mode_str,
                                         R"({"mode":")" + mode_str + "\"}");
                }
                return make_response(static_cast<int>(rc),
                                     response_code_name(rc));
            } catch (const std::exception& e) {
                return make_response(1, std::string("Parse error: ") + e.what());
            }
        }

        if (method == "GET" && path == "/GetMode") {
            Mode m = ModeController::instance().active_mode();
            return make_response(0, "Success",
                                 R"({"mode":")" + std::string(mode_name(m)) + "\"}");
        }

        if (method == "POST" && path == "/SetParameter") {
            try {
                json req = json::parse(body);
                std::string module = req.value("module", "core");

                if (module == "core") {
                    if (req.contains("parameters") && req["parameters"].is_array()) {
                        for (const auto& p : req["parameters"]) {
                            std::string name = p.value("key", p.value("name", ""));
                            std::string value;
                            if (p.contains("value")) {
                                if (p["value"].is_string())
                                    value = p["value"].get<std::string>();
                                else
                                    value = p["value"].dump();
                            }
                            if (name == "ai_mode") {
                                cfg.set_string("api2a.ai_mode", value);
                                ai_mode = ai_mode_from_string(value);
                            } else if (name == "processing_paused") {
                                g_processing_paused.store(value == "true" || value == "1");
                            } else if (!name.empty()) {
                                cfg.set_string(name, value);
                            }
                        }
                    }
                    return make_response(0, "Core parameters updated");
                } else if (module == "stereo_camera") {
                    return sc_client.send_command("POST", "/SetParameter", body);
                } else if (module == "ai") {
                    return ai_client.send_command("SetParameter",
                        req.contains("parameters") ? req["parameters"].dump() : "");
                }
                return make_response(4, "Unknown module: " + module);
            } catch (const std::exception& e) {
                return make_response(1, e.what());
            }
        }

        if (method == "POST" && path == "/GetParameter") {
            try {
                json req = json::parse(body);
                std::string module = req.value("module", "core");

                if (module == "core") {
                    json data;
                    data["ai_mode"] = ai_mode_name(ai_mode);
                    data["active_mode"] = mode_name(ModeController::instance().active_mode());
                    data["processing_paused"] = g_processing_paused.load();
                    return make_response(0, "Success", data.dump());
                } else if (module == "stereo_camera") {
                    return sc_client.send_command("POST", "/GetParameter", body);
                } else if (module == "ai") {
                    return ai_client.send_command("GetParameter",
                        req.contains("parameters") ? req["parameters"].dump() : "");
                }
                return make_response(4, "Unknown module: " + module);
            } catch (const std::exception& e) {
                return make_response(1, e.what());
            }
        }

        if (method == "POST" && path == "/StartInspection") {
            try {
                json req = json::parse(body);
                std::string station = req.value("station_id", "");
                std::string escalator = req.value("escalator_id", "");
                std::string task = req.value("task_id", "T1");

                if (station.empty() || escalator.empty()) {
                    return make_response(4, "station_id and escalator_id are required");
                }

                auto rc = ModeController::instance().set_mode(Mode::Inspection);
                if (rc != ResponseCode::Success && rc != ResponseCode::AlreadyInit) {
                    return make_response(static_cast<int>(rc), response_code_name(rc));
                }
                task_mgr.start_inspection(station, escalator, task);
                StatusTracker::instance().set_task_active(true);

                std::string record_path = db.records().create_inspection_record(
                    station, escalator, task);
                db.records().set_active_record(record_path);

                preprocess.start_inspection(record_path);

                return make_response(0, "Inspection started",
                                     R"({"record_path":")" + record_path + "\"}");
            } catch (const std::exception& e) {
                return make_response(1, e.what());
            }
        }

        if (method == "POST" && path == "/StopInspection") {
            preprocess.stop_inspection();
            task_mgr.stop();
            StatusTracker::instance().set_task_active(false);
            db.records().set_active_record("");
            ModeController::instance().set_mode(Mode::None);
            return make_response(0, "Inspection stopped");
        }

        if (method == "POST" && path == "/SetSubTask") {
            try {
                json req = json::parse(body);
                std::string st = req.value("sub_task", "");
                if (st == "installation") {
                    preprocess.set_sub_task(InspectionSubTask::Installation);
                } else if (st == "marking") {
                    preprocess.set_sub_task(InspectionSubTask::Marking);
                } else if (st == "gap_inspection") {
                    preprocess.set_sub_task(InspectionSubTask::GapInspection);
                } else {
                    return make_response(4, "Invalid sub_task: " + st);
                }
                return make_response(0, "Sub-task set to " + st);
            } catch (const std::exception& e) {
                return make_response(1, e.what());
            }
        }

        if (method == "POST" && path == "/StartAITest") {
            try {
                json req = json::parse(body);
                std::string test_data = req.value("test_data_path", "");

                auto rc2 = ModeController::instance().set_mode(Mode::AITest);
                if (rc2 != ResponseCode::Success && rc2 != ResponseCode::AlreadyInit) {
                    return make_response(static_cast<int>(rc2), response_code_name(rc2));
                }
                task_mgr.start_ai_test(test_data);
                StatusTracker::instance().set_task_active(true);

                std::string record_path = db.records().create_ai_test_record();
                db.records().set_active_record(record_path);

                preprocess.start_ai_test(test_data);

                return make_response(0, "AI test started",
                                     R"({"record_path":")" + record_path + "\"}");
            } catch (const std::exception& e) {
                return make_response(1, e.what());
            }
        }

        if (method == "POST" && path == "/StereoCameraCommand") {
            try {
                json req = json::parse(body);
                std::string cmd = req.value("command", "CheckStatus");
                std::string params = req.contains("params") ? req["params"].dump() : "{}";
                return sc_client.send_command("POST", "/" + cmd, params);
            } catch (const std::exception& e) {
                return make_response(1, e.what());
            }
        }

        if (method == "POST" && path == "/AICommand") {
            try {
                json req = json::parse(body);
                std::string action = req.value("action", "CheckStatus");
                std::string params = req.contains("params") ? req["params"].dump() : "";
                return ai_client.send_command(action, params);
            } catch (const std::exception& e) {
                return make_response(1, e.what());
            }
        }

        return make_response(5, "Unavailable: " + method + " " + path);
    });

    client_server.start(cfg.get_string("api3a.host", "0.0.0.0"),
                        cfg.get_int("api3a.port", 9445),
                        cfg.get_string("api3a.cert_path", "certs/server.crt"),
                        cfg.get_string("api3a.key_path", "certs/server.key"),
                        cfg.get_int("api3a.worker_threads", 4));

    wss_server.start(cfg.get_string("api3c.host", "127.0.0.1"),
                     cfg.get_int("api3c.port", 9446),
                     cfg.get_string("api3c.cert_path", "certs/server.crt"),
                     cfg.get_string("api3c.key_path", "certs/server.key"),
                     cfg.get_int("api3c.worker_threads", 2));

    StatusTracker::instance().set_stereo_camera_connected(true);
    StatusTracker::instance().set_ai_dealer_connected(dealer.is_running());
    StatusTracker::instance().set_ai_dealer_mode(ai_mode_name(ai_mode));

    Logger::info("=== ECIDS Core running ===");

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    Logger::info("=== ECIDS Core shutting down ===");

    preprocess.stop();
    subscriber.stop();
    dealer.stop();
    publisher.stop();
    wss_server.stop();
    client_server.stop();
    db.housekeeper().stop();

    Logger::info("=== ECIDS Core stopped ===");
    return 0;
}
