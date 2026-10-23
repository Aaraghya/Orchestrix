#pragma once

#include "AllocationStrategy.h"
#include "DataCenter.h"
#include "Job.h"
#include "Server.h"

#include <string>

/// Coordinates job-to-server allocation using a pluggable AllocationStrategy.
///
/// Responsibilities (architecture.md §3.7):
///  - Ask the configured strategy to select a server.
///  - Validate and reserve resources through the Server interface if a
///    candidate is found.
///  - Transition the job to RUNNING state on success.
///  - Leave the job QUEUED and touch no server state if no server is feasible.
///
/// What the Scheduler does NOT do:
///  - Advance the simulation clock.
///  - Maintain the queue of waiting jobs (that belongs to the SimulationEngine).
///  - Duplicate the resource-accounting logic in Server (it calls Server methods).
///
/// Relationship to ResourceManager (future Milestone):
///  In the documented architecture (§3.5), ResourceManager sits between the
///  Scheduler and the Server and provides a named, trackable reservation handle.
///  That component is deferred to the simulation-engine milestone.  For now the
///  Scheduler calls Server::reserveResources() directly, which provides the same
///  atomicity guarantee (simulation-rules.md §4).  The interface is designed so
///  that a ResourceManager can be inserted later without changing callers.
///
/// Thread safety: not required (single-threaded simulation).
class Scheduler {
public:
    /// Constructs a Scheduler with a strategy and a data center.
    ///
    /// \param strategy  Non-owning reference to an allocation strategy.
    ///                  The strategy must outlive the Scheduler.
    /// \param dataCenter Non-owning reference to the DataCenter.
    ///                  The DataCenter must outlive the Scheduler.
    Scheduler(const AllocationStrategy& strategy, DataCenter& dataCenter);

    /// Attempts to allocate a server for \p job at \p currentTick.
    ///
    /// Pre-conditions:
    ///  - job.getState() == JobState::QUEUED.
    ///  - currentTick >= job.getArrivalTick().
    ///
    /// On success:
    ///  - Resources are reserved on the chosen server.
    ///  - job.start(serverId, currentTick) is called.
    ///  - Returns a non-owning pointer to the server the job was placed on.
    ///
    /// On failure (no feasible server):
    ///  - No server state is modified.
    ///  - job remains QUEUED.
    ///  - Returns nullptr.
    ///
    /// Throws std::logic_error when pre-conditions are violated.
    [[nodiscard]] Server* scheduleJob(Job& job, int currentTick);

    /// Returns the name of the currently configured strategy.
    [[nodiscard]] std::string strategyName() const;

    /// Swaps the allocation strategy at runtime (useful for strategy-comparison
    /// runs in the SimulationEngine milestone).
    void setStrategy(const AllocationStrategy& strategy) noexcept;

private:
    const AllocationStrategy* m_strategy;
    DataCenter*               m_dataCenter;
};
