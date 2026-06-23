/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: DataBuffer implementation
 * Date: 2026-06-18
 * Modification: 2026-06-23 Drop-newest policy, added depth map queue
 */

#include "ecids_core/data/DataBuffer.h"

namespace ecids_core {

void DataBuffer::put(const std::string& channel, const DataBundle& bundle) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& entry = channels_[channel];
    entry.header = bundle.header;
    entry.data = bundle.data;
}

bool DataBuffer::get_latest(const std::string& channel, DataBundle& out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = channels_.find(channel);
    if (it == channels_.end()) return false;
    out.header = it->second.header;
    out.data = it->second.data;
    return true;
}

bool DataBuffer::get_latest_copy(const std::string& channel, FrameHeader& header,
                                 std::shared_ptr<std::vector<uint8_t>>& data) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = channels_.find(channel);
    if (it == channels_.end()) return false;
    header = it->second.header;
    data = it->second.data;
    return true;
}

void DataBuffer::put_stereo(uint64_t pair_id, const std::string& part, const DataBundle& bundle) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& pair = stereo_pairs_[pair_id];
    if (part == "left") {
        pair.left = bundle;
        pair.has_left = true;
    } else if (part == "right") {
        pair.right = bundle;
        pair.has_right = true;
    }
}

bool DataBuffer::try_get_stereo_pair(uint64_t pair_id, DataBundle& left, DataBundle& right) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = stereo_pairs_.find(pair_id);
    if (it == stereo_pairs_.end()) return false;
    if (!it->second.has_left || !it->second.has_right) return false;
    left = it->second.left;
    right = it->second.right;
    stereo_pairs_.erase(it);
    return true;
}

void DataBuffer::enqueue_inspection(const DataBundle& bundle) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (inspection_queue_.size() >= inspection_capacity_) {
        return;
    }
    inspection_queue_.push_back(bundle);
    inspection_cv_.notify_one();
}

bool DataBuffer::dequeue_inspection(DataBundle& out, int timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (timeout_ms <= 0) {
        if (inspection_queue_.empty()) return false;
    } else {
        inspection_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
            [this] { return !inspection_queue_.empty(); });
        if (inspection_queue_.empty()) return false;
    }
    out = inspection_queue_.front();
    inspection_queue_.pop_front();
    return true;
}

void DataBuffer::enqueue_depth(const DataBundle& bundle) {
    std::lock_guard<std::mutex> lock(mutex_);
    latest_depth_ = bundle;
    has_depth_ = true;
}

bool DataBuffer::dequeue_depth(DataBundle& out, int timeout_ms) {
    (void)timeout_ms;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!has_depth_) return false;
    out = latest_depth_;
    return true;
}

std::vector<std::string> DataBuffer::channel_names() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    names.reserve(channels_.size());
    for (const auto& kv : channels_) {
        names.push_back(kv.first);
    }
    return names;
}

void DataBuffer::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    channels_.clear();
    stereo_pairs_.clear();
}

void DataBuffer::clear_inspection_queue() {
    std::lock_guard<std::mutex> lock(mutex_);
    inspection_queue_.clear();
}

size_t DataBuffer::inspection_queue_size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return inspection_queue_.size();
}

} // namespace ecids_core
