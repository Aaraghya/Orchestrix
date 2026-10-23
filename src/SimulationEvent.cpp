#include "SimulationEvent.h"

std::string eventTypeToString(EventType type) {
    switch (type) {
        case EventType::JOB_ARRIVED:       return "JOB_ARRIVED";
        case EventType::JOB_REJECTED:      return "JOB_REJECTED";
        case EventType::JOB_STARTED:       return "JOB_STARTED";
        case EventType::JOB_COMPLETED:     return "JOB_COMPLETED";
        case EventType::JOB_FAILED:        return "JOB_FAILED";
        case EventType::JOB_CANCELLED:     return "JOB_CANCELLED";
        case EventType::RESOURCE_RELEASED: return "RESOURCE_RELEASED";
        case EventType::SIM_STARTED:       return "SIM_STARTED";
        case EventType::SIM_FINISHED:      return "SIM_FINISHED";
        case EventType::SIM_MAX_TICK:      return "SIM_MAX_TICK";
    }
    return "UNKNOWN";
}
