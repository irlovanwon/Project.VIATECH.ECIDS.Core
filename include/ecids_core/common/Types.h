/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Shared type definitions for internal data transfer
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented full type system for dataflow
 */

#ifndef ECIDS_CORE_COMMON_TYPES_H
#define ECIDS_CORE_COMMON_TYPES_H

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <unordered_map>

namespace ecids_core {

enum class Mode {
    None = 0,
    Config = 1,
    Inspection = 2,
    Data = 3,
    Calibration = 4,
    Uploading = 5,
    Installation = 6,
    AITest = 7
};

inline const char* mode_name(Mode m) {
    switch (m) {
        case Mode::None:         return "none";
        case Mode::Config:       return "config";
        case Mode::Inspection:   return "inspection";
        case Mode::Data:         return "data";
        case Mode::Calibration:  return "calibration";
        case Mode::Uploading:    return "uploading";
        case Mode::Installation: return "installation";
        case Mode::AITest:       return "ai_test";
        default:                 return "unknown";
    }
}

inline Mode mode_from_string(const std::string& s) {
    if (s == "config")       return Mode::Config;
    if (s == "inspection")   return Mode::Inspection;
    if (s == "data")         return Mode::Data;
    if (s == "calibration")  return Mode::Calibration;
    if (s == "uploading")    return Mode::Uploading;
    if (s == "installation") return Mode::Installation;
    if (s == "ai_test")      return Mode::AITest;
    return Mode::None;
}

inline bool mode_is_exclusive(Mode m) {
    return m != Mode::None && m != Mode::Uploading;
}

enum class AIMode { File, Http, Binary };

inline const char* ai_mode_name(AIMode m) {
    switch (m) {
        case AIMode::File:   return "file";
        case AIMode::Http:   return "http";
        case AIMode::Binary: return "binary";
        default:             return "file";
    }
}

inline AIMode ai_mode_from_string(const std::string& s) {
    if (s == "http")   return AIMode::Http;
    if (s == "binary") return AIMode::Binary;
    return AIMode::File;
}

struct FrameHeader {
    int type = 0;
    int64_t ts_sec = 0;
    int64_t ts_nsec = 0;
    uint64_t pair_id = 0;
    std::string part;
    std::string channel;
};

struct DataBundle {
    FrameHeader header;
    std::shared_ptr<std::vector<uint8_t>> data;

    DataBundle() : data(std::make_shared<std::vector<uint8_t>>()) {}

    std::string timestamp_str() const {
        char buf[32];
        snprintf(buf, sizeof(buf), "%ld-%09ld", (long)header.ts_sec, (long)header.ts_nsec);
        return std::string(buf);
    }
};

struct Detection {
    std::string label_id;
    double confidence = 0.0;
    std::string file_name;
    std::vector<std::pair<double, double>> coordinates;
    std::string ts_start;
    std::string ts_end;
};

struct DetectionResponse {
    std::string transaction_id;
    std::string dealer_id;
    std::vector<Detection> results;
    std::string ts_received;
    std::string ts_replied;
};

struct InspectionResult {
    std::string transaction_id;
    std::string task_id;
    std::string station_id;
    std::string escalator_id;
    double gap_distance_mm = 0.0;
    std::vector<Detection> ai_detections;
    std::vector<Detection> abnormal;
    std::string timestamp;
    std::vector<std::string> image_files;
};

} // namespace ecids_core

#endif // ECIDS_CORE_COMMON_TYPES_H
