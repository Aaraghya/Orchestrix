#pragma once

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
