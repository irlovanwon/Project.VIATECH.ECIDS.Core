/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: API1b StereoCameraClient implementation
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented with libcurl
 */

#include "ecids_core/api1/StereoCameraClient.h"
#include "ecids_core/common/Logger.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace ecids_core {

using json = nlohmann::json;

static size_t write_cb_(void* ptr, size_t size, size_t nmemb, std::string* data) {
    data->append(static_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}

void StereoCameraClient::init(const std::string& host, int port, int timeout_ms) {
    host_ = host;
    port_ = port;
    timeout_ms_ = timeout_ms;
    Logger::info("StereoCameraClient: " + host + ":" + std::to_string(port));
}

std::string StereoCameraClient::send_command(const std::string& method,
                                             const std::string& path,
                                             const std::string& body) {
    std::lock_guard<std::mutex> lock(mutex_);

    CURL* curl = curl_easy_init();
    if (!curl) {
        Logger::error("StereoCameraClient: curl init failed");
        return R"({"code":1,"message":"curl init failed"})";
    }

    std::string url = "https://" + host_ + ":" + std::to_string(port_) + path;
    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms_);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb_);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    } else {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        Logger::error("StereoCameraClient: " + url + " failed: " + curl_easy_strerror(res));
        return R"({"code":1,"message":")" + std::string(curl_easy_strerror(res)) + R"("})";
    }

    return response;
}

std::string StereoCameraClient::check_status() {
    return send_command("GET", "/CheckStatus");
}

std::string StereoCameraClient::connect() {
    return send_command("POST", "/Connect");
}

std::string StereoCameraClient::disconnect() {
    return send_command("POST", "/Disconnect");
}

std::string StereoCameraClient::start_capture(const std::string& data_types_json) {
    return send_command("POST", "/StartCapture", data_types_json);
}

std::string StereoCameraClient::stop_capture(const std::string& data_types_json) {
    return send_command("POST", "/StopCapture", data_types_json);
}

std::string StereoCameraClient::set_parameter(const std::string& name, const std::string& value) {
    json body;
    body["command"] = "set_parameter";
    json params;
    params["name"] = name;
    params["value"] = value;
    body["params"] = params;
    return send_command("POST", "/SetParameter", body.dump());
}

std::string StereoCameraClient::get_parameter(const std::string& name) {
    json body;
    body["command"] = "get_parameter";
    body["params"] = {{"name", name}};
    return send_command("POST", "/GetParameter", body.dump());
}

std::string StereoCameraClient::activate_channel(const std::string& channel) {
    json body;
    body["command"] = "activate_channel";
    body["params"] = {{"channel", channel}};
    return send_command("POST", "/SetParameter", body.dump());
}

std::string StereoCameraClient::deactivate_channel(const std::string& channel) {
    json body;
    body["command"] = "deactivate_channel";
    body["params"] = {{"channel", channel}};
    return send_command("POST", "/SetParameter", body.dump());
}

} // namespace ecids_core
