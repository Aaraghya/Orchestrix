#pragma once

#include <string>

/// The kind of event recorded during a simulation run.
enum class EventType {
    JOB_ARRIVED,        ///< A job reached its arrival tick and became QUEUED.
    JOB_REJECTED,       ///< A job was rejected as permanently infeasible.
    JOB_STARTED,        ///< A job was allocated and started RUNNING.
    JOB_COMPLETED,      ///< A job finished execution and became COMPLETED.
    JOB_FAILED,         ///< A job terminated with FAILED status.
    JOB_CANCELLED,      ///< A job was explicitly cancelled.
    RESOURCE_RELEASED,  ///< Resources were released back to a server.
    SIM_STARTED,        ///< The simulation run began.
    SIM_FINISHED,       ///< The simulation run ended normally.
    SIM_MAX_TICK        ///< The simulation stopped due to the tick limit.
};

/// Returns a short human-readable label for an EventType.
[[nodiscard]] std::string eventTypeToString(EventType type);

/// A single simulation event recorded during a run.
///
/// Fields that are not applicable to a particular event type are left empty.
struct SimulationEvent {
    int         tick     = 0;
    EventType   type     = EventType::SIM_STARTED;
    std::string jobId;      ///< ID of the affected job, or "" if none.
    std::string serverId;   ///< ID of the affected server, or "" if none.
    std::string detail;     ///< Optional human-readable detail string.
};
