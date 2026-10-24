#include "ReportGenerator.h"

#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>

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
    out << "      Queued    : " << m.queued          << "\n";
    out << "      Running   : " << m.running         << "\n";

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

    const std::string divider(55, '-');
    out << divider << "\n";
    out << "  Job Outcome Summary (Final Job Counts)\n";
    out << divider << "\n";
    out << "  " << std::left  << std::setw(15) << "Strategy"
        << std::right
        << std::setw(10) << "Completed"
        << std::setw(10) << "Rejected"
        << std::setw(9)  << "Queued"
        << std::setw(9)  << "Running"
        << "\n";
    out << "  " << std::string(53, '-') << "\n";
    for (const auto& m : results) {
        out << "  " << std::left  << std::setw(15) << m.strategyName
            << std::right
            << std::setw(10) << m.completed
            << std::setw(10) << m.rejected
            << std::setw(9)  << m.queued
            << std::setw(9)  << m.running
            << "\n";
    }
    out << divider << "\n";

    return out.str();
}

std::string ReportGenerator::generateFilteredEvents(
        const std::vector<SimulationEvent>& events,
        EventType filterType)
{
    std::ostringstream out;
    const std::string divider(60, '-');

    out << divider << "\n";
    out << "  Simulation Events: " << eventTypeToString(filterType) << "\n";
    out << divider << "\n";

    out << "  " << std::left
        << std::setw(8)  << "Tick"
        << std::setw(12) << "Job ID"
        << std::setw(12) << "Server ID"
        << "Detail" << "\n";
    out << "  " << std::string(58, '-') << "\n";

    int count = 0;
    for (const auto& ev : events) {
        if (ev.type == filterType) {
            ++count;
            out << "  " << std::left
                << std::setw(8)  << ev.tick
                << std::setw(12) << (ev.jobId.empty() ? "-" : ev.jobId)
                << std::setw(12) << (ev.serverId.empty() ? "-" : ev.serverId)
                << ev.detail << "\n";
        }
    }

    if (count == 0) {
        out << "  (No matching events found)\n";
    }

    out << divider << "\n";
    out << "  Total matching events: " << count << "\n";
    out << divider << "\n";

    return out.str();
}

// ── Server status report ──────────────────────────────────────────────────────

std::string ReportGenerator::generateServerStatus(
        const DataCenter&                   dc,
        const std::vector<SimulationEvent>& events,
        const std::vector<Job>&             jobs)
{
    // Build a map from job id → requirements so we can apply/undo reservations.
    std::unordered_map<std::string, ResourceRequirements> reqByJob;
    reqByJob.reserve(jobs.size());
    for (const auto& j : jobs) {
        reqByJob.emplace(j.getId(), j.getRequirements());
    }

    // Snapshot: available resources per server (id → ResourceRequirements).
    // Initialised to each server's total capacity (nothing allocated yet).
    const auto& servers = dc.getServers();
    std::map<std::string, ResourceRequirements> avail;   // ordered by id
    // We also need type name and capacity for the header row.
    struct ServerMeta { std::string type; ResourceRequirements capacity; };
    std::map<std::string, ServerMeta> meta;

    for (const auto& sptr : servers) {
        avail[sptr->getId()]  = sptr->getCapacity();
        meta[sptr->getId()]   = { sptr->typeName(), sptr->getCapacity() };
    }

    // Column widths for the table.
    const int W_ID   = 8;
    const int W_TYPE = 14;
    const int W_VAL  = 8;   // each numeric cell (capacity/available)

    // Helper: format a double with one decimal place in a fixed-width field.
    auto fmtVal = [&](double v) -> std::string {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << v;
        return ss.str();
    };

    // Helper: emit one full table (header + one row per server).
    // Prints:  ID | Type | Cap CPU | Avl CPU | Cap RAM | Avl RAM | ...
    auto printTable = [&](std::ostringstream& out) {
        // Header row.
        out << "  " << std::left
            << std::setw(W_ID)   << "Server"
            << std::setw(W_TYPE) << "Type"
            << std::setw(W_VAL)  << "Cap.CPU"
            << std::setw(W_VAL)  << "Avl.CPU"
            << std::setw(W_VAL)  << "Cap.RAM"
            << std::setw(W_VAL)  << "Avl.RAM"
            << std::setw(W_VAL+2)<< "Cap.Stor"
            << std::setw(W_VAL+2)<< "Avl.Stor"
            << std::setw(W_VAL)  << "Cap.GPU"
            << std::setw(W_VAL)  << "Avl.GPU"
            << "\n";
        // Divider.
        out << "  " << std::string(W_ID + W_TYPE + W_VAL*8 + 4, '-') << "\n";
        // One row per server.
        for (const auto& [id, a] : avail) {
            const ResourceRequirements& cap = meta[id].capacity;
            out << "  " << std::left
                << std::setw(W_ID)   << id
                << std::setw(W_TYPE) << meta[id].type
                << std::setw(W_VAL)  << fmtVal(cap.cpu)
                << std::setw(W_VAL)  << fmtVal(a.cpu)
                << std::setw(W_VAL)  << fmtVal(cap.ram)
                << std::setw(W_VAL)  << fmtVal(a.ram)
                << std::setw(W_VAL+2)<< fmtVal(cap.storage)
                << std::setw(W_VAL+2)<< fmtVal(a.storage)
                << std::setw(W_VAL)  << fmtVal(cap.gpu)
                << std::setw(W_VAL)  << fmtVal(a.gpu)
                << "\n";
        }
    };

    std::ostringstream out;
    const std::string header(55, '=');

    out << header << "\n";
    out << "  Server Resource Status\n";
    out << header << "\n";
    out << "  (Avl = available; Cap = total capacity; units: cores / GB / devices)\n\n";

    // Snapshot 0: initial state (nothing allocated).
    out << "  [tick 0 — initial state]\n";
    printTable(out);

    // Replay JOB_STARTED and RESOURCE_RELEASED events in log order.
    // JOB_STARTED  → subtract requirements from the affected server.
    // RESOURCE_RELEASED → add requirements back.
    for (const auto& ev : events) {
        if (ev.type == EventType::JOB_STARTED) {
            const auto jit = reqByJob.find(ev.jobId);
            const auto sit = avail.find(ev.serverId);
            if (jit != reqByJob.end() && sit != avail.end()) {
                const ResourceRequirements& req = jit->second;
                sit->second.cpu     -= req.cpu;
                sit->second.ram     -= req.ram;
                sit->second.storage -= req.storage;
                sit->second.gpu     -= req.gpu;
                out << "\n  [tick " << ev.tick << " — job " << ev.jobId
                    << " started on " << ev.serverId << "]\n";
                printTable(out);
            }
        } else if (ev.type == EventType::RESOURCE_RELEASED) {
            const auto jit = reqByJob.find(ev.jobId);
            const auto sit = avail.find(ev.serverId);
            if (jit != reqByJob.end() && sit != avail.end()) {
                const ResourceRequirements& req = jit->second;
                sit->second.cpu     += req.cpu;
                sit->second.ram     += req.ram;
                sit->second.storage += req.storage;
                sit->second.gpu     += req.gpu;
                out << "\n  [tick " << ev.tick << " — job " << ev.jobId
                    << " released resources from " << ev.serverId << "]\n";
                printTable(out);
            }
        }
    }

    out << header << "\n";
    return out.str();
}
