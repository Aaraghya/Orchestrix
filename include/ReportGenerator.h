#pragma once

#include "DataCenter.h"
#include "Job.h"
#include "SimulationEvent.h"
#include "SimulationMetrics.h"

#include <string>
#include <vector>

/// Converts SimulationMetrics into human-readable text reports.
///
/// Design: all methods are static; no state is stored.
/// Reporting is kept entirely separate from metric calculation.
class ReportGenerator {
public:
    /// Produces a report for a single simulation run.
    ///
    /// Included sections:
    ///   - Strategy name and simulation status.
    ///   - Elapsed ticks.
    ///   - Job outcome counts and percentages.
    ///   - Average waiting time (or N/A).
    ///   - Throughput (or N/A).
    ///   - CPU, RAM, storage, and GPU utilization (or N/A).
    ///
    /// Undefined metrics are shown as "N/A".
    [[nodiscard]] static std::string generate(const SimulationMetrics& m);

    /// Produces a side-by-side comparison report for multiple strategies.
    ///
    /// Prints one section per strategy using generate(), separated by dividers.
    [[nodiscard]] static std::string generateComparison(
        const std::vector<SimulationMetrics>& results);

    /// Produces a server resource-status report from a completed simulation.
    ///
    /// Replays JOB_STARTED and RESOURCE_RELEASED events from the engine's
    /// event log to reconstruct how each server's available CPU, RAM, storage,
    /// and GPU change during the run.  The report shows:
    ///   - An "Initial" snapshot (all resources free).
    ///   - One snapshot after each JOB_STARTED event (resources consumed).
    ///   - One snapshot after each RESOURCE_RELEASED event (resources freed).
    ///
    /// This makes the dynamic allocation pattern visible without requiring any
    /// engine modification or additional runtime state.
    ///
    /// Parameters
    ///   dc      — the DataCenter used for the run (server ids, types, and
    ///             total capacities; must outlive this call).
    ///   events  — the event log from SimulationEngine::getEvents().
    ///   jobs    — the job list from SimulationEngine::getJobs() (used to
    ///             look up resource requirements by job id).
    [[nodiscard]] static std::string generateServerStatus(
        const DataCenter&                   dc,
        const std::vector<SimulationEvent>& events,
        const std::vector<Job>&             jobs);

private:
    ReportGenerator() = delete;

    /// Formats an optional<double> as a percentage string or "N/A".
    /// Use for values already stored as percentages (0-100).
    [[nodiscard]] static std::string fmtPct(const std::optional<double>& v);

    /// Formats an optional<double> that is a [0,1] fraction as a percentage
    /// string (multiplies by 100) or "N/A". Used for resource utilization.
    [[nodiscard]] static std::string fmtFracAsPct(const std::optional<double>& v);

    /// Formats an optional<double> with a given suffix or "N/A".
    [[nodiscard]] static std::string fmtOpt(const std::optional<double>& v,
                                            const std::string& suffix = "");
};
