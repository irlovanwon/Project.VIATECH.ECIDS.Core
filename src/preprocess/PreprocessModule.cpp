/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: PreprocessModule implementation
 * Date: 2026-06-18
 * Modification: 2026-06-18 Added run_ai_test for testing data folder input
 */

#include "ecids_core/preprocess/PreprocessModule.h"
#include "ecids_core/common/Logger.h"

namespace ecids_core {

void PreprocessModule::run_inspection() {
    // TODO: Transfer image from API1a to API2a for AI processing
}

void PreprocessModule::run_ai_test(const std::string& test_data_path) {
    Logger::info("AI Test: reading images from " + test_data_path);
    // TODO: Iterate images in test_data_path
    //       Transfer each image from testing data folder to API2a for AI processing
    //       Get AI results from API2a
    //       Transfer AI result + image data to postprocessing
}

void PreprocessModule::send_to_ai(const std::string& image_uri) {
    // TODO: Package and send image to API2a (DetectionDealer)
    (void)image_uri;
}

void PreprocessModule::receive_ai_results() {
    // TODO: Receive detection results from API2a and forward to postprocessing
}

} // namespace ecids_core
