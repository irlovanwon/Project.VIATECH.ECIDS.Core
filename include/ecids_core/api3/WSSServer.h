/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: API3c WSS server - web client data (One Loop Per Thread)
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented WebSocket over TLS with topic subscription
 */

#ifndef ECIDS_CORE_API3_WSSSERVER_H
#define ECIDS_CORE_API3_WSSSERVER_H

#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <cstdint>

namespace ecids_core {

class WSSServer {
public:
    WSSServer();
    ~WSSServer();

    void start(const std::string& host, int port,
               const std::string& cert_path, const std::string& key_path,
               int worker_threads = 2);
    void stop();

    void broadcast_json(const std::string& topic, const std::string& json);
    void broadcast_binary(const std::string& topic,
                          const uint8_t* data, size_t size,
                          const std::string& header_json = "");

    bool is_running() const { return running_; }

private:
    void accept_loop_();
    void client_loop_(int fd, void* ssl);

    bool do_handshake_(void* ssl);
    bool send_ws_text_(void* ssl, const std::string& text);
    bool send_ws_binary_(void* ssl, const uint8_t* data, size_t size);
    std::string recv_ws_text_(void* ssl);

    static std::string compute_accept_key_(const std::string& ws_key);

    int listen_fd_ = -1;
    void* ssl_ctx_ = nullptr;
    std::thread accept_thread_;
    std::atomic<bool> running_{false};

    struct ClientConn {
        void* ssl = nullptr;
        int fd = -1;
        std::mutex write_mutex;
        std::vector<std::string> topics;
        bool alive = true;
    };

    std::mutex clients_mutex_;
    std::vector<ClientConn*> clients_;
};

} // namespace ecids_core

#endif // ECIDS_CORE_API3_WSSSERVER_H
