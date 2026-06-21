/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: API3a HTTPS server - client commands (Hybrid Pools)
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented epoll + thread-per-request HTTPS server
 */

#ifndef ECIDS_CORE_API3_CLIENTSERVER_H
#define ECIDS_CORE_API3_CLIENTSERVER_H

#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace ecids_core {

class ClientServer {
public:
    using ForwardHandler = std::function<std::string(
        const std::string& method, const std::string& path,
        const std::string& body)>;

    ClientServer();
    ~ClientServer();

    void set_forward_handler(ForwardHandler handler);

    void start(const std::string& host, int port,
               const std::string& cert_path, const std::string& key_path,
               int worker_threads = 4);
    void stop();

    bool is_running() const { return running_; }

private:
    void accept_loop_();
    void worker_loop_();
    void handle_client_(int fd);

    std::string read_request_(void* ssl);
    void write_response_(void* ssl, const std::string& response);

    std::string route_(const std::string& method, const std::string& path,
                       const std::string& body);

    int listen_fd_ = -1;
    void* ssl_ctx_ = nullptr;
    std::thread accept_thread_;
    std::atomic<bool> running_{false};

    std::vector<std::thread> workers_;
    std::queue<int> client_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    ForwardHandler forward_handler_;
};

} // namespace ecids_core

#endif // ECIDS_CORE_API3_CLIENTSERVER_H
