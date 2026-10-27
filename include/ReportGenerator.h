#pragma once

#include "DataCenter.h"
#include "Job.h"
#include "SimulationEvent.h"
#include "SimulationMetrics.h"

#include <iosfwd>
#include <string>
#include <vector>

class SimulationEngine;

/// A single record in the allocation details table.
struct AllocationRecord {
    std::string jobId;
    std::string serverId;  ///< Assigned server ID, or "-" if none.
    std::string status;    ///< Job outcome status ("Allocated", "Rejected", "Queued", etc.).
};

/// Summary metrics displayed at the top of the simulation results screen.
struct ResultsSummary {
    std::string strategyName;          ///< e.g. "First Fit", "Best Fit", "Load Balanced".
    int         jobsProcessed = 0;     ///< Total jobs submitted / processed.
    int         jobsAllocated = 0;     ///< Jobs successfully allocated / completed.
    int         jobsRejected  = 0;     ///< Jobs rejected as infeasible.
    std::string avgAllocationTime;     ///< "N/A" (allocation time is not measured).
    std::string resourceUtilization;   ///< e.g. "CPU: 9.9% | RAM: 8.2% | Storage: 3.2% | GPU: 7.2%".
};

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

    /// Produces a formatted report of simulation events matching a given event type.
    [[nodiscard]] static std::string generateFilteredEvents(
        const std::vector<SimulationEvent>& events,
        EventType filterType);

    /// Normalizes internal strategy identifiers ("FirstFit") into display format ("First Fit").
    [[nodiscard]] static std::string formatStrategyName(const std::string& name);

    /// Builds a ResultsSummary from SimulationMetrics.
    [[nodiscard]] static ResultsSummary buildResultsSummary(const SimulationMetrics& m);

    /// Reconstructs allocation records from a job list and simulation event log.
    [[nodiscard]] static std::vector<AllocationRecord> buildAllocationRecords(
        const std::vector<Job>&             jobs,
        const std::vector<SimulationEvent>& events);

    /// Reconstructs allocation records from a finished SimulationEngine.
    [[nodiscard]] static std::vector<AllocationRecord> buildAllocationRecords(
        const SimulationEngine& engine);

    /// Formats a single page of the paginated simulation results screen.
    ///
    /// \param summary    Simulation summary information.
    /// \param records    Complete collection of job allocation records.
    /// \param page       1-indexed page number.
    /// \param pageSize   Maximum number of records per page (default 20).
    ///                   Values below 1 are clamped to 1.
    [[nodiscard]] static std::string generateResultsPage(
        const ResultsSummary&                summary,
        const std::vector<AllocationRecord>& records,
        int                                  page,
        int                                  pageSize = 20);

    /// Interactive pagination loop.
    ///
    /// Reads navigation commands from \p in and prints pages to \p out.
    /// Supports N/n (next page), P/p (previous page), and Q/q (quit / return to menu).
    /// Handles invalid commands and EOF safely.  A non-positive \p pageSize is
    /// clamped to 1.
    ///
    /// \return true if the user exited normally via 'Q'/'q', false if EOF was reached.
    static bool showPaginatedResults(
        std::istream&                        in,
        std::ostream&                        out,
        const ResultsSummary&                summary,
        const std::vector<AllocationRecord>& records,
        int                                  pageSize = 20);

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