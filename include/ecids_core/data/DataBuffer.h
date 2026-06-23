/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Internal ring buffer (shared_ptr) — thread-safe per-channel data store
 * Date: 2026-06-18
 * Modification: 2026-06-23 Changed inspection queue to drop-newest policy
 */

#ifndef ECIDS_CORE_DATA_DATABUFFER_H
#define ECIDS_CORE_DATA_DATABUFFER_H

#pragma once

#include "ecids_core/common/Types.h"
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <unordered_map>
#include <atomic>

namespace ecids_core {

class DataBuffer {
public:
    void set_inspection_capacity(size_t cap) { inspection_capacity_ = cap; }

    void put(const std::string& channel, const DataBundle& bundle);

    bool get_latest(const std::string& channel, DataBundle& out) const;

    bool get_latest_copy(const std::string& channel, FrameHeader& header,
                         std::shared_ptr<std::vector<uint8_t>>& data) const;

    void put_stereo(uint64_t pair_id, const std::string& part, const DataBundle& bundle);
    bool try_get_stereo_pair(uint64_t pair_id, DataBundle& left, DataBundle& right);

    void enqueue_inspection(const DataBundle& bundle);
    bool dequeue_inspection(DataBundle& out, int timeout_ms = 0);

    void enqueue_depth(const DataBundle& bundle);
    bool dequeue_depth(DataBundle& out, int timeout_ms = 0);

    std::vector<std::string> channel_names() const;
    void clear();
    void clear_inspection_queue();

    size_t inspection_queue_size() const;

private:
    struct ChannelEntry {
        FrameHeader header;
        std::shared_ptr<std::vector<uint8_t>> data;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, ChannelEntry> channels_;

    struct StereoPair {
        DataBundle left;
        DataBundle right;
        bool has_left = false;
        bool has_right = false;
    };
    std::unordered_map<uint64_t, StereoPair> stereo_pairs_;

    std::deque<DataBundle> inspection_queue_;
    size_t inspection_capacity_ = 100;
    std::condition_variable inspection_cv_;

    DataBundle latest_depth_;
    bool has_depth_ = false;
};

} // namespace ecids_core

#endif // ECIDS_CORE_DATA_DATABUFFER_H
