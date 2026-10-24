#include "ReportGenerator.h"

#include <iomanip>
#include <sstream>
#include <string>

// ── Private helpers ───────────────────────────────────────────────────────────

std::string ReportGenerator::fmtPct(const std::optional<double>& v) {
    if (!v.has_value()) return "N/A";
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << v.value() << "%";
    return ss.str();
}

/// Formats an optional<double> that is a [0,1] fraction as a percentage string
/// (multiplies by 100 before printing) or "N/A".
std::string ReportGenerator::fmtFracAsPct(const std::optional<double>& v) {
    if (!v.has_value()) return "N/A";
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << (v.value() * 100.0) << "%";
    return ss.str();
}

std::string ReportGenerator::fmtOpt(const std::optional<double>& v,
                                    const std::string& suffix) {
    if (!v.has_value()) return "N/A";
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3) << v.value();
    if (!suffix.empty()) ss << " " << suffix;
    return ss.str();
}

// ── Public API ────────────────────────────────────────────────────────────────

std::string ReportGenerator::generate(const SimulationMetrics& m) {
    std::ostringstream out;

    const std::string divider(55, '-');
    out << divider << "\n";
    out << "  Strategy  : " << m.strategyName << "\n";
    out << "  Status    : ";
    if (m.hitMaxTick)         out << "STOPPED (max-tick limit reached)\n";
    else if (m.finishedNormally) out << "Completed normally\n";
    else                         out << "Stopped\n";
    out << "  Elapsed   : " << m.elapsedTicks << " tick(s)\n";

    out << "\n  Job Outcomes\n";
    out << "    Submitted   : " << m.totalSubmitted  << "\n";
    out << "    Completed   : " << m.completed       << "  (" << fmtPct(m.completionRate) << ")\n";
    out << "    Rejected    : " << m.rejected        << "  (" << fmtPct(m.rejectionRate)  << ")\n";
    out << "    Cancelled   : " << m.cancelled       << "\n";
    out << "    Failed      : " << m.failed          << "\n";
    out << "    Still active: " << m.stillActive     << "\n";

    out << "\n  Performance\n";
    out << "    Avg wait time  : ";
    if (m.avgWaitingTime.has_value()) {
        std::ostringstream t;
        t << std::fixed << std::setprecision(3) << m.avgWaitingTime.value()
          << " tick(s)  [sample n=" << m.waitingTimeSampleCount << "]";
        out << t.str() << "\n";
    } else {
        out << "N/A  (no job started)\n";
    }
    out << "    Throughput     : " << fmtOpt(m.throughput, "jobs/tick") << "\n";

    out << "\n  Resource Utilization (resource-time accounting)\n";
    out << "    CPU     : " << fmtFracAsPct(m.cpuUtilization)     << "\n";
    out << "    RAM     : " << fmtFracAsPct(m.ramUtilization)     << "\n";
    out << "    Storage : " << fmtFracAsPct(m.storageUtilization) << "\n";
    out << "    GPU     : " << fmtFracAsPct(m.gpuUtilization)     << "\n";
    out << divider << "\n";

    return out.str();
}

std::string ReportGenerator::generateComparison(
        const std::vector<SimulationMetrics>& results)
{
    std::ostringstream out;
    const std::string header(55, '=');

    out << header << "\n";
    out << "  Strategy Comparison Report\n";
    out << header << "\n\n";

    for (const auto& m : results) {
        out << generate(m);
        out << "\n";
    }

    return out.str();
}
