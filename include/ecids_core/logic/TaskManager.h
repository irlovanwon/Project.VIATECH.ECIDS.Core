/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: Inspection/installation/AI-test task state
 * Date: 2026-06-18
 * Modification: 2026-06-18 Added test_data_path for AI Test mode
 */

#ifndef ECIDS_CORE_LOGIC_TASKMANAGER_H
#define ECIDS_CORE_LOGIC_TASKMANAGER_H

#pragma once

#include <string>

namespace ecids_core {

enum class TaskType {
    None,
    Inspection,
    Installation,
    AITest
};

struct InspectionTask {
    std::string station_id;
    std::string escalator_id;
    std::string task_id;    // "T1" or "T2"
};

struct AITestTask {
    std::string test_data_path;  // Path to testing data folder on remote server
};

class TaskManager {
public:
    TaskType type() const { return type_; }
    const InspectionTask& inspection() const { return inspection_; }
    const AITestTask& ai_test() const { return ai_test_; }

    void start_inspection(const std::string& station_id,
                          const std::string& escalator_id,
                          const std::string& task_id);
    void start_installation(const std::string& station_id,
                            const std::string& escalator_id);
    void start_ai_test(const std::string& test_data_path);
    void stop();

private:
    TaskType type_ = TaskType::None;
    InspectionTask inspection_;
    AITestTask ai_test_;
};

} // namespace ecids_core

#endif // ECIDS_CORE_LOGIC_TASKMANAGER_H
