/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: API3c WSSServer implementation
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented WebSocket over TLS
 */

#include "ecids_core/api3/WSSServer.h"
#include "ecids_core/common/Logger.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/evp.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <nlohmann/json.hpp>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <cerrno>
#include <ctime>
#include <ctime>

namespace ecids_core {

static const char WS_MAGIC[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static std::string base64_encode_(const uint8_t* data, size_t len) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, mem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, data, static_cast<int>(len));
    BIO_flush(b64);
    BUF_MEM* bptr;
    BIO_get_mem_ptr(b64, &bptr);
    std::string result(bptr->data, bptr->length);
    BIO_free_all(b64);
    return result;
}

std::string WSSServer::compute_accept_key_(const std::string& ws_key) {
    std::string combined = ws_key + WS_MAGIC;
    unsigned char sha1_out[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(combined.c_str()),
         combined.size(), sha1_out);
    return base64_encode_(sha1_out, SHA_DIGEST_LENGTH);
}

WSSServer::WSSServer() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
}

WSSServer::~WSSServer() {
    stop();
}

void WSSServer::start(const std::string& host, int port,
                      const std::string& cert_path, const std::string& key_path,
                      int worker_threads) {
    (void)worker_threads;

    ssl_ctx_ = SSL_CTX_new(TLS_server_method());
    if (!ssl_ctx_) {
        Logger::error("WSSServer: SSL_CTX_new failed");
        return;
    }

    if (SSL_CTX_use_certificate_file(static_cast<SSL_CTX*>(ssl_ctx_), cert_path.c_str(), SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(static_cast<SSL_CTX*>(ssl_ctx_), key_path.c_str(), SSL_FILETYPE_PEM) <= 0) {
        Logger::error("WSSServer: cert/key load failed");
        SSL_CTX_free(static_cast<SSL_CTX*>(ssl_ctx_));
        ssl_ctx_ = nullptr;
        return;
    }

    listen_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (listen_fd_ < 0) return;

    int reuse = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    if (bind(listen_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        Logger::error("WSSServer: bind failed: " + std::string(std::strerror(errno)));
        close(listen_fd_);
        listen_fd_ = -1;
        return;
    }

    if (listen(listen_fd_, 64) < 0) {
        Logger::error("WSSServer: listen failed");
        close(listen_fd_);
        listen_fd_ = -1;
        return;
    }

    running_ = true;
    accept_thread_ = std::thread(&WSSServer::accept_loop_, this);
    Logger::info("WSSServer: listening on " + host + ":" + std::to_string(port));
}

void WSSServer::stop() {
    if (!running_) return;
    running_ = false;

    if (listen_fd_ >= 0) {
        close(listen_fd_);
        listen_fd_ = -1;
    }

    if (accept_thread_.joinable()) accept_thread_.join();

    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto* c : clients_) {
            if (c->ssl) { SSL_shutdown(static_cast<SSL*>(c->ssl)); SSL_free(static_cast<SSL*>(c->ssl)); }
            if (c->fd >= 0) close(c->fd);
            c->alive = false;
            delete c;
        }
        clients_.clear();
    }

    if (ssl_ctx_) {
        SSL_CTX_free(static_cast<SSL_CTX*>(ssl_ctx_));
        ssl_ctx_ = nullptr;
    }
    Logger::info("WSSServer: stopped");
}

void WSSServer::accept_loop_() {
    while (running_) {
        struct sockaddr_in caddr{};
        socklen_t clen = sizeof(caddr);
        int fd = accept4(listen_fd_, (struct sockaddr*)&caddr, &clen, SOCK_NONBLOCK);
        if (fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(200000);
                continue;
            }
            continue;
        }

        SSL* ssl = SSL_new(static_cast<SSL_CTX*>(ssl_ctx_));
        if (!ssl) { close(fd); continue; }
        SSL_set_fd(ssl, fd);
        SSL_set_accept_state(ssl);

        if (!do_handshake_(ssl)) {
            SSL_free(ssl);
            close(fd);
            continue;
        }

        auto* conn = new ClientConn();
        conn->ssl = ssl;
        conn->fd = fd;

        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            clients_.push_back(conn);
        }

        std::thread(&WSSServer::client_loop_, this, fd, ssl).detach();
        Logger::info("WSSServer: client connected fd=" + std::to_string(fd));
    }
}

bool WSSServer::do_handshake_(void* ssl_void) {
    SSL* ssl = static_cast<SSL*>(ssl_void);

    if (SSL_accept(ssl) <= 0) {
        int err = SSL_get_error(ssl, -1);
        if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
            return false;
        }
        int rc = SSL_accept(ssl);
        if (rc <= 0) return false;
    }

    char buf[4096];
    std::string request;
    while (true) {
        int n = SSL_read(ssl, buf, sizeof(buf));
        if (n > 0) {
            request.append(buf, n);
            if (request.find("\r\n\r\n") != std::string::npos) break;
        } else {
            int err = SSL_get_error(ssl, n);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) continue;
            break;
        }
    }

    if (request.find("Upgrade: websocket") == std::string::npos &&
        request.find("upgrade: websocket") == std::string::npos) {
        return false;
    }

    std::string ws_key;
    {
        std::istringstream ss(request);
        std::string line;
        while (std::getline(ss, line)) {
            size_t pos = line.find("Sec-WebSocket-Key:");
            if (pos == std::string::npos)
                pos = line.find("sec-websocket-key:");
            if (pos != std::string::npos) {
                ws_key = line.substr(pos + 18);
                while (!ws_key.empty() && (ws_key.front() == ' ' || ws_key.front() == '\t'))
                    ws_key.erase(0, 1);
                while (!ws_key.empty() && (ws_key.back() == '\r' || ws_key.back() == '\n'))
                    ws_key.pop_back();
                break;
            }
        }
    }

    if (ws_key.empty()) return false;

    std::string accept_key = compute_accept_key_(ws_key);
    std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept_key + "\r\n"
        "\r\n";

    size_t offset = 0;
    while (offset < response.size()) {
        int n = SSL_write(ssl, response.data() + offset,
                          static_cast<int>(response.size() - offset));
        if (n > 0) offset += n;
        else break;
    }

    return offset == response.size();
}

bool WSSServer::send_ws_text_(void* ssl_void, const std::string& text) {
    SSL* ssl = static_cast<SSL*>(ssl_void);
    size_t len = text.size();
    std::vector<uint8_t> frame;

    frame.push_back(0x81); // FIN + text frame

    if (len <= 125) {
        frame.push_back(static_cast<uint8_t>(len));
    } else if (len <= 65535) {
        frame.push_back(126);
        frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(len & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<uint8_t>((len >> (i * 8)) & 0xFF));
        }
    }

    frame.insert(frame.end(), text.begin(), text.end());

    int total = 0;
    while (total < static_cast<int>(frame.size())) {
        int n = SSL_write(ssl, frame.data() + total,
                          static_cast<int>(frame.size()) - total);
        if (n <= 0) return false;
        total += n;
    }
    return true;
}

bool WSSServer::send_ws_binary_(void* ssl_void, const uint8_t* data, size_t size) {
    SSL* ssl = static_cast<SSL*>(ssl_void);
    std::vector<uint8_t> frame;

    frame.push_back(0x82); // FIN + binary frame

    if (size <= 125) {
        frame.push_back(static_cast<uint8_t>(size));
    } else if (size <= 65535) {
        frame.push_back(126);
        frame.push_back(static_cast<uint8_t>((size >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(size & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<uint8_t>((size >> (i * 8)) & 0xFF));
        }
    }

    frame.insert(frame.end(), data, data + size);

    int total = 0;
    while (total < static_cast<int>(frame.size())) {
        int n = SSL_write(ssl, frame.data() + total,
                          static_cast<int>(frame.size()) - total);
        if (n <= 0) return false;
        total += n;
    }
    return true;
}

std::string WSSServer::recv_ws_text_(void* ssl_void) {
    SSL* ssl = static_cast<SSL*>(ssl_void);

    uint8_t hdr[2];
    int n = SSL_read(ssl, hdr, 2);
    if (n < 2) return "";

    bool masked = (hdr[1] & 0x80) != 0;
    uint64_t payload_len = hdr[1] & 0x7F;

    if (payload_len == 126) {
        uint8_t ext[2];
        SSL_read(ssl, ext, 2);
        payload_len = (ext[0] << 8) | ext[1];
    } else if (payload_len == 127) {
        uint8_t ext[8];
        SSL_read(ssl, ext, 8);
        payload_len = 0;
        for (int i = 0; i < 8; ++i) {
            payload_len = (payload_len << 8) | ext[i];
        }
    }

    uint8_t mask[4] = {0, 0, 0, 0};
    if (masked) {
        SSL_read(ssl, mask, 4);
    }

    if (payload_len > 1024 * 1024) return "";

    std::string payload;
    payload.resize(payload_len);
    size_t read_so_far = 0;
    while (read_so_far < payload_len) {
        int r = SSL_read(ssl, &payload[read_so_far],
                         static_cast<int>(payload_len - read_so_far));
        if (r <= 0) return "";
        read_so_far += r;
    }

    if (masked) {
        for (size_t i = 0; i < payload_len; ++i) {
            payload[i] ^= mask[i % 4];
        }
    }

    return payload;
}

void WSSServer::client_loop_(int fd, void* ssl_void) {
    SSL* ssl = static_cast<SSL*>(ssl_void);

    ClientConn* conn = nullptr;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto* c : clients_) {
            if (c->fd == fd) { conn = c; break; }
        }
    }
    if (!conn) return;

    while (running_ && conn->alive) {
        std::string msg = recv_ws_text_(ssl);
        if (msg.empty()) break;

        try {
            nlohmann::json j = nlohmann::json::parse(msg);
            std::string action = j.value("action", "");
            if (action == "subscribe") {
                if (j.contains("topics") && j["topics"].is_array()) {
                    for (const auto& t : j["topics"]) {
                        conn->topics.push_back(t.get<std::string>());
                    }
                }
            } else if (action == "unsubscribe") {
                if (j.contains("topics") && j["topics"].is_array()) {
                    for (const auto& t : j["topics"]) {
                        conn->topics.erase(
                            std::remove(conn->topics.begin(), conn->topics.end(),
                                        t.get<std::string>()),
                            conn->topics.end());
                    }
                }
            }
        } catch (...) {}
    }

    conn->alive = false;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_.erase(std::remove(clients_.begin(), clients_.end(), conn), clients_.end());
    }
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(fd);
    delete conn;
    Logger::info("WSSServer: client disconnected fd=" + std::to_string(fd));
}

void WSSServer::broadcast_json(const std::string& topic, const std::string& json) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (auto* conn : clients_) {
        if (!conn->alive || !conn->ssl) continue;
        bool subscribed = false;
        for (const auto& t : conn->topics) {
            if (t == topic || t == "core/*") { subscribed = true; break; }
        }
        if (!subscribed) continue;

        nlohmann::json frame;
        frame["topic"] = topic;
        frame["ts_sec"] = static_cast<int64_t>(time(nullptr));
        frame["data"] = nlohmann::json::parse(json, nullptr, false);

        std::lock_guard<std::mutex> wlock(conn->write_mutex);
        send_ws_text_(conn->ssl, frame.dump());
    }
}

void WSSServer::broadcast_binary(const std::string& topic,
                                 const uint8_t* data, size_t size,
                                 const std::string& header_json) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (auto* conn : clients_) {
        if (!conn->alive || !conn->ssl) continue;
        bool subscribed = false;
        for (const auto& t : conn->topics) {
            if (t == topic || t == "core/*") { subscribed = true; break; }
        }
        if (!subscribed) continue;

        std::lock_guard<std::mutex> wlock(conn->write_mutex);
        send_ws_binary_(conn->ssl, data, size);
    }
    (void)header_json;
}

} // namespace ecids_core
