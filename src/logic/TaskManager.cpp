/*
 * Copyright(c) 2026-2030, VIATECH & UZONE All rights reserved
 * Des: TaskManager implementation
 * Date: 2026-06-18
 * Modification: 2026-06-21 Implemented
 */

#include "ecids_core/logic/TaskManager.h"
#include "ecids_core/common/Logger.h"

namespace ecids_core {

void TaskManager::start_inspection(const std::string& station_id,
                                   const std::string& escalator_id,
                                   const std::string& task_id) {
    type_ = TaskType::Inspection;
    inspection_.station_id = station_id;
    inspection_.escalator_id = escalator_id;
    inspection_.task_id = task_id;
    ai_test_.test_data_path.clear();
    Logger::info("TaskManager: inspection started — station=" + station_id
                 + " escalator=" + escalator_id + " task=" + task_id);
}

void TaskManager::start_installation(const std::string& station_id,
                                     const std::string& escalator_id) {
    type_ = TaskType::Installation;
    inspection_.station_id = station_id;
    inspection_.escalator_id = escalator_id;
    inspection_.task_id = "T1";
    ai_test_.test_data_path.clear();
    Logger::info("TaskManager: installation started — station=" + station_id
                 + " escalator=" + escalator_id);
}

void TaskManager::start_ai_test(const std::string& test_data_path) {
    type_ = TaskType::AITest;
    ai_test_.test_data_path = test_data_path;
    inspection_.station_id.clear();
    inspection_.escalator_id.clear();
    inspection_.task_id.clear();
    Logger::info("TaskManager: AI test started — path=" + test_data_path);
}

void TaskManager::stop() {
    switch (type_) {
        case TaskType::Inspection:
            Logger::info("TaskManager: inspection stopped");
            break;
        case TaskType::Installation:
            Logger::info("TaskManager: installation stopped");
            break;
        case TaskType::AITest:
            Logger::info("TaskManager: AI test stopped");
            break;
        default:
            break;
    }
    type_ = TaskType::None;
    inspection_ = InspectionTask{};
    ai_test_ = AITestTask{};
}

} // namespace ecids_core
