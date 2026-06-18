/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: ECIDS Core module entry point
 * Date: 2026-06-18
 * Modification:
 */

#include <iostream>
#include <string>
#include <csignal>
#include <atomic>
#include <chrono>
#include <thread>

#include "ecids_core/common/Logger.h"
#include "ecids_core/common/Response.h"

static std::atomic<bool> g_running{true};

static void signal_handler(int sig) {
    (void)sig;
    g_running = false;
}

int main(int argc, char* argv[]) {
    std::string config_dir = (argc > 1) ? argv[1] : "config";
    std::string cert_path  = (argc > 2) ? argv[2] : "certs/server.crt";
    std::string key_path   = (argc > 3) ? argv[3] : "certs/server.key";

    (void)cert_path;
    (void)key_path;

    ecids_core::Logger::info("ECIDS Core starting...");
    ecids_core::Logger::info("Config dir: " + config_dir);

    // TODO: Initialize ConfigManager, API modules, Logic, Pre/Postprocess, Database

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    ecids_core::Logger::info("ECIDS Core running.");

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ecids_core::Logger::info("ECIDS Core shutting down...");
    return 0;
}
