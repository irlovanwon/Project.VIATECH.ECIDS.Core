/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: API3a ClientServer implementation — Hybrid Pools HTTPS
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented epoll accept + thread-per-request
 */

#include "ecids_core/api3/ClientServer.h"
#include "ecids_core/common/Logger.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include <cstring>
#include <sstream>
#include <cerrno>

namespace ecids_core {

static bool ensure_certs_(const std::string& cert_path, const std::string& key_path);

ClientServer::ClientServer() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
}

ClientServer::~ClientServer() {
    stop();
}

void ClientServer::set_forward_handler(ForwardHandler handler) {
    forward_handler_ = std::move(handler);
}

void ClientServer::start(const std::string& host, int port,
                         const std::string& cert_path, const std::string& key_path,
                         int worker_threads) {
    ensure_certs_(cert_path, key_path);

    ssl_ctx_ = SSL_CTX_new(TLS_server_method());
    if (!ssl_ctx_) {
        Logger::error("ClientServer: SSL_CTX_new failed");
        return;
    }

    if (SSL_CTX_use_certificate_file(static_cast<SSL_CTX*>(ssl_ctx_), cert_path.c_str(), SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(static_cast<SSL_CTX*>(ssl_ctx_), key_path.c_str(), SSL_FILETYPE_PEM) <= 0) {
        Logger::error("ClientServer: cert/key load failed");
        SSL_CTX_free(static_cast<SSL_CTX*>(ssl_ctx_));
        ssl_ctx_ = nullptr;
        return;
    }

    listen_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (listen_fd_ < 0) {
        Logger::error("ClientServer: socket failed");
        return;
    }

    int reuse = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    if (bind(listen_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        Logger::error("ClientServer: bind " + host + ":" + std::to_string(port) + " failed: "
                      + std::strerror(errno));
        close(listen_fd_);
        listen_fd_ = -1;
        return;
    }

    if (listen(listen_fd_, 128) < 0) {
        Logger::error("ClientServer: listen failed");
        close(listen_fd_);
        listen_fd_ = -1;
        return;
    }

    running_ = true;

    for (int i = 0; i < worker_threads; ++i) {
        workers_.emplace_back(&ClientServer::worker_loop_, this);
    }

    accept_thread_ = std::thread(&ClientServer::accept_loop_, this);
    Logger::info("ClientServer: listening on " + host + ":" + std::to_string(port)
                 + " (" + std::to_string(worker_threads) + " workers)");
}

void ClientServer::stop() {
    if (!running_) return;
    running_ = false;

    if (listen_fd_ >= 0) {
        close(listen_fd_);
        listen_fd_ = -1;
    }

    queue_cv_.notify_all();
    if (accept_thread_.joinable()) accept_thread_.join();

    for (auto& w : workers_) {
        if (w.joinable()) w.join();
    }
    workers_.clear();

    if (ssl_ctx_) {
        SSL_CTX_free(static_cast<SSL_CTX*>(ssl_ctx_));
        ssl_ctx_ = nullptr;
    }
    Logger::info("ClientServer: stopped");
}

void ClientServer::accept_loop_() {
    int epfd = epoll_create1(0);
    struct epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd_;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd_, &ev);

    struct epoll_event events[16];

    while (running_) {
        int n = epoll_wait(epfd, events, 16, 200);
        if (n <= 0) continue;

        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd != listen_fd_) continue;

            struct sockaddr_in caddr{};
            socklen_t clen = sizeof(caddr);
            int client_fd = accept4(listen_fd_, (struct sockaddr*)&caddr, &clen,
                                    SOCK_NONBLOCK);
            if (client_fd < 0) continue;

            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                if (client_queue_.size() >= 64) {
                    close(client_fd);
                    continue;
                }
                client_queue_.push(client_fd);
            }
            queue_cv_.notify_one();
        }
    }

    close(epfd);
}

void ClientServer::worker_loop_() {
    while (running_) {
        int fd = -1;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait_for(lock, std::chrono::milliseconds(200),
                [this] { return !running_ || !client_queue_.empty(); });
            if (!running_ && client_queue_.empty()) break;
            if (client_queue_.empty()) continue;
            fd = client_queue_.front();
            client_queue_.pop();
        }
        handle_client_(fd);
    }
}

void ClientServer::handle_client_(int fd) {
    SSL* ssl = SSL_new(static_cast<SSL_CTX*>(ssl_ctx_));
    if (!ssl) {
        close(fd);
        return;
    }
    SSL_set_fd(ssl, fd);
    SSL_set_accept_state(ssl);

    int rc = SSL_accept(ssl);
    if (rc <= 0) {
        int err = SSL_get_error(ssl, rc);
        if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
            SSL_free(ssl);
            close(fd);
            return;
        }
    }

    std::string raw = read_request_(ssl);
    if (raw.empty()) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(fd);
        return;
    }

    size_t hdr_end = raw.find("\r\n\r\n");
    if (hdr_end == std::string::npos) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(fd);
        return;
    }

    std::string headers = raw.substr(0, hdr_end);
    std::string body = raw.substr(hdr_end + 4);

    std::string method, path;
    {
        std::istringstream ss(headers);
        std::string line;
        if (std::getline(ss, line)) {
            size_t sp1 = line.find(' ');
            size_t sp2 = line.rfind(' ');
            if (sp1 != std::string::npos && sp2 != std::string::npos) {
                method = line.substr(0, sp1);
                path = line.substr(sp1 + 1, sp2 - sp1 - 1);
            }
        }
    }

    std::string response_body = route_(method, path, body);

    std::string http_response = "HTTP/1.1 200 OK\r\n";
    http_response += "Content-Type: application/json\r\n";
    http_response += "Content-Length: " + std::to_string(response_body.size()) + "\r\n";
    http_response += "Connection: close\r\n";
    http_response += "\r\n";
    http_response += response_body;

    write_response_(ssl, http_response);

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(fd);
}

std::string ClientServer::read_request_(void* ssl_void) {
    SSL* ssl = static_cast<SSL*>(ssl_void);
    std::string buf;
    char tmp[4096];

    while (true) {
        int n = SSL_read(ssl, tmp, sizeof(tmp));
        if (n > 0) {
            buf.append(tmp, n);
            if (buf.find("\r\n\r\n") != std::string::npos) break;
        } else {
            int err = SSL_get_error(ssl, n);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                continue;
            }
            break;
        }
    }

    size_t hdr_end = buf.find("\r\n\r\n");
    if (hdr_end == std::string::npos) return buf;

    std::string headers = buf.substr(0, hdr_end);

    size_t content_length = 0;
    {
        std::string lower;
        lower.reserve(headers.size());
        for (char c : headers) lower += static_cast<char>(tolower(c));
        size_t pos = lower.find("content-length:");
        if (pos != std::string::npos) {
            size_t val_start = pos + 15;
            while (val_start < lower.size() && (lower[val_start] == ' ' || lower[val_start] == '\t'))
                val_start++;
            try {
                content_length = std::stoul(headers.substr(val_start));
            } catch (...) {}
        }
    }

    size_t body_start = hdr_end + 4;
    while (buf.size() - body_start < content_length) {
        int n = SSL_read(ssl, tmp, sizeof(tmp));
        if (n > 0) {
            buf.append(tmp, n);
        } else {
            int err = SSL_get_error(ssl, n);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                continue;
            }
            break;
        }
    }

    return buf;
}

void ClientServer::write_response_(void* ssl_void, const std::string& response) {
    SSL* ssl = static_cast<SSL*>(ssl_void);
    size_t offset = 0;
    while (offset < response.size()) {
        int n = SSL_write(ssl, response.data() + offset,
                          static_cast<int>(response.size() - offset));
        if (n > 0) {
            offset += n;
        } else {
            int err = SSL_get_error(ssl, n);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                continue;
            }
            break;
        }
    }
}

std::string ClientServer::route_(const std::string& method, const std::string& path,
                                 const std::string& body) {
    if (forward_handler_) {
        return forward_handler_(method, path, body);
    }
    return R"({"code":1,"message":"No handler configured"})";
}

static bool ensure_certs_(const std::string& cert_path, const std::string& key_path) {
    FILE* f = fopen(cert_path.c_str(), "r");
    if (f) { fclose(f); return true; }

    Logger::info("ClientServer: generating self-signed cert");
    EVP_PKEY* pkey = EVP_PKEY_new();
    BIGNUM* bn = BN_new();
    BN_set_word(bn, RSA_F4);
    RSA* rsa = RSA_new();
    RSA_generate_key_ex(rsa, 2048, bn, nullptr);
    EVP_PKEY_assign_RSA(pkey, rsa);
    BN_free(bn);

    X509* x509 = X509_new();
    ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), 31536000L);
    X509_set_pubkey(x509, pkey);

    X509_NAME* name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC,
        (unsigned char*)"HK", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC,
        (unsigned char*)"VIATECH", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
        (unsigned char*)"ECIDS-Core", -1, -1, 0);
    X509_set_issuer_name(x509, name);

    X509_sign(x509, pkey, EVP_sha256());

    FILE* fp = fopen(cert_path.c_str(), "wb");
    if (fp) {
        PEM_write_X509(fp, x509);
        fclose(fp);
    }
    fp = fopen(key_path.c_str(), "wb");
    if (fp) {
        PEM_write_PrivateKey(fp, pkey, nullptr, nullptr, 0, nullptr, nullptr);
        fclose(fp);
    }

    X509_free(x509);
    EVP_PKEY_free(pkey);
    return true;
}

} // namespace ecids_core
