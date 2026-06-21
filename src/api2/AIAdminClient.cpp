/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: API2b AIAdminClient implementation
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented with libcurl
 */

#include "ecids_core/api2/AIAdminClient.h"
#include "ecids_core/common/Logger.h"
#include "ecids_core/common/Timestamp.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace ecids_core {

using json = nlohmann::json;

static size_t write_cb_(void* ptr, size_t size, size_t nmemb, std::string* data) {
    data->append(static_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}

void AIAdminClient::init(const std::string& host, int port, int timeout_ms) {
    host_ = host;
    port_ = port;
    timeout_ms_ = timeout_ms;
    Logger::info("AIAdminClient: " + host + ":" + std::to_string(port));
}

std::string AIAdminClient::send_command(const std::string& action,
                                        const std::string& params_json) {
    std::lock_guard<std::mutex> lock(mutex_);

    json body;
    body["TransactionID"] = Timestamp::now_string();
    body["DealerID"] = "ecids_core";
    body["Action"] = action;

    if (!params_json.empty()) {
        try {
            body["Parameter"] = json::parse(params_json);
        } catch (...) {
            body["Parameter"] = json::array();
        }
    }
    body["Timestamp"] = Timestamp::now_string().substr(0, 15);

    std::string url = "https://" + host_ + ":" + std::to_string(port_) + "/";
    std::string payload = body.dump();
    std::string response;

    CURL* curl = curl_easy_init();
    if (!curl) return R"({"code":1,"message":"curl init failed"})";

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms_);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb_);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        Logger::error("AIAdminClient: " + url + " failed: " + curl_easy_strerror(res));
        return R"({"code":1,"message":")" + std::string(curl_easy_strerror(res)) + R"("})";
    }

    return response;
}

std::string AIAdminClient::check_status() {
    return send_command("CheckStatus");
}

std::string AIAdminClient::set_parameter(const std::string& name, const std::string& value) {
    json params = json::array({json{{"ID", name}, {"Value", value}}});
    return send_command("SetParameter", params.dump());
}

std::string AIAdminClient::get_parameter(const std::string& name) {
    json params = json::array({json{{"ID", name}}});
    return send_command("GetParameter", params.dump());
}

} // namespace ecids_core
