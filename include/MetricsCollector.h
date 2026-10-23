#pragma once

#include "DataCenter.h"
#include "SimulationEngine.h"
#include "SimulationMetrics.h"

#include <string>

/// Computes SimulationMetrics from a completed simulation run.
///
/// Design decisions
/// ────────────────
/// The collector is stateless (all methods static) so it can be called on any
/// completed engine without owning it.
///
/// Resource-time accounting (simulation-rules.md §12.4):
///   The event log already contains:
///     JOB_STARTED   — tick, jobId, serverId
///     RESOURCE_RELEASED — tick, jobId, serverId  (fired before JOB_COMPLETED)
///   For each job that started, the interval [startTick, finishTick) is
///   reconstructed by matching JOB_STARTED with RESOURCE_RELEASED.
///   Jobs still RUNNING at the simulation stop have no RESOURCE_RELEASED event;
///   engine.currentTick() is used as their pseudo-finish-tick.
///
/// No engine modifications are needed: all data is already in the event log
/// and the job objects.
class MetricsCollector {
public:
    /// Computes and returns metrics for a completed simulation run.
    ///
    /// \param engine      A finished SimulationEngine (run() must have been called).
    /// \param dataCenter  The DataCenter used for the run (server capacities).
    /// \param strategyName Human-readable name of the strategy used.
    ///
    /// Throws std::logic_error if the engine has not finished.
    [[nodiscard]] static SimulationMetrics collect(
        const SimulationEngine& engine,
        const DataCenter&       dataCenter,
        const std::string&      strategyName);

private:
    MetricsCollector() = delete;  // all-static utility class
};
