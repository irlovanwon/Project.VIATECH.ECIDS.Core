/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Image to AI pipeline orchestration
 * Date: 2026-06-18
 * Modification: 2026-06-18 Added run_ai_test for testing data folder input
 */

#ifndef ECIDS_CORE_PREPROCESS_PREPROCESSMODULE_H
#define ECIDS_CORE_PREPROCESS_PREPROCESSMODULE_H

#pragma once

#include <string>

namespace ecids_core {

class PreprocessModule {
public:
    // Inspection/Installation: images come from API1a (ZMQ SUB from StereoCamera)
    void run_inspection();

    // AI Test: images come from a testing data folder on the remote server
    // instead of live StereoCamera feed
    void run_ai_test(const std::string& test_data_path);

private:
    // Transfer image data to API2a for AI processing
    void send_to_ai(const std::string& image_uri);

    // Receive AI results from API2a and forward to postprocessing
    void receive_ai_results();
};

} // namespace ecids_core

#endif // ECIDS_CORE_PREPROCESS_PREPROCESSMODULE_H
