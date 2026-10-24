#include "MetricsCollector.h"
#include "SimulationEvent.h"

#include <stdexcept>
#include <unordered_map>

SimulationMetrics MetricsCollector::collect(
        const SimulationEngine& engine,
        const DataCenter&       dataCenter,
        const std::string&      strategyName)
{
    if (!engine.hasFinished()) {
        throw std::logic_error(
            "MetricsCollector::collect: engine has not finished running.");
    }

    const auto& jobs   = engine.getJobs();
    const auto& events = engine.getEvents();
    const int   elapsed = engine.currentTick();

    SimulationMetrics m;
    m.strategyName     = strategyName;
    m.elapsedTicks     = elapsed;
    m.hitMaxTick       = engine.hitMaxTick();
    m.finishedNormally = !engine.hitMaxTick();

    // ── Job outcome counts ────────────────────────────────────────────────────

    m.totalSubmitted = static_cast<int>(jobs.size());

    for (const auto& job : jobs) {
        switch (job.getState()) {
            case JobState::COMPLETED:  ++m.completed;                    break;
            case JobState::REJECTED:   ++m.rejected;                     break;
            case JobState::CANCELLED:  ++m.cancelled;                    break;
            case JobState::FAILED:     ++m.failed;                       break;
            case JobState::QUEUED:     ++m.stillActive; ++m.queued;      break;
            case JobState::RUNNING:    ++m.stillActive; ++m.running;     break;
            case JobState::PENDING:    ++m.stillActive;                  break;
        }
    }

    if (m.totalSubmitted > 0) {
        m.completionRate = (static_cast<double>(m.completed) / m.totalSubmitted) * 100.0;
        m.rejectionRate  = (static_cast<double>(m.rejected)  / m.totalSubmitted) * 100.0;
    }
    // else both remain nullopt (division by zero avoided)

    // ── Waiting time ──────────────────────────────────────────────────────────
    // Waiting time = startTick - arrivalTick for each job that actually started.
    // We read startTick from Job::getStartTick() and arrivalTick from
    // Job::getArrivalTick().  Only jobs that have a startTick contributed.

    double waitSum   = 0.0;
    int    waitCount = 0;

    for (const auto& job : jobs) {
        if (job.getStartTick().has_value()) {
            waitSum += static_cast<double>(job.getStartTick().value() - job.getArrivalTick());
            ++waitCount;
        }
    }

    m.waitingTimeSampleCount = waitCount;
    if (waitCount > 0) {
        m.avgWaitingTime = waitSum / static_cast<double>(waitCount);
    }
    // else nullopt: no job started

    // ── Throughput ────────────────────────────────────────────────────────────
    // throughput = completed / elapsedTicks
    // Undefined (nullopt) when elapsedTicks == 0.

    if (elapsed > 0) {
        m.throughput = static_cast<double>(m.completed) / static_cast<double>(elapsed);
    }

    // ── Resource-time utilization ─────────────────────────────────────────────
    //
    // Build a per-job interval map from the event log:
    //   JOB_STARTED     → {jobId, serverId, startTick}
    //   RESOURCE_RELEASED → {jobId, serverId, finishTick}  (fired before JOB_COMPLETED)
    //
    // For jobs still RUNNING at simulation stop, RESOURCE_RELEASED was never
    // fired; use engine.currentTick() as finishTick.
    //
    // allocated_time_dim  = Σ req_dim × (finishTick − startTick)  over started jobs
    // total_avail_time_dim = Σ cap_dim × elapsedTicks              over all servers
    // utilization_dim     = allocated_time_dim / total_avail_time_dim
    // Reported as nullopt when total_avail_time_dim == 0.

    struct Interval {
        std::string serverId;
        int         startTick = 0;
        int         finishTick = -1;  // -1 means "not yet set"
    };

    std::unordered_map<std::string, Interval> intervals; // jobId → interval

    for (const auto& ev : events) {
        if (ev.type == EventType::JOB_STARTED) {
            intervals[ev.jobId] = { ev.serverId, ev.tick, -1 };
        } else if (ev.type == EventType::RESOURCE_RELEASED) {
            auto it = intervals.find(ev.jobId);
            if (it != intervals.end() && it->second.finishTick == -1) {
                it->second.finishTick = ev.tick;
            }
        }
    }

    // For jobs with no RESOURCE_RELEASED (still running at stop), use elapsed.
    for (auto& [jobId, iv] : intervals) {
        if (iv.finishTick == -1) {
            iv.finishTick = elapsed;
        }
    }

    // Build a lookup from serverId → requirements for started jobs.
    // (We need the server's *allocation requirement* per job.)
    // We get requirements from the Job objects.
    std::unordered_map<std::string, const ResourceRequirements*> jobReqMap;
    for (const auto& job : jobs) {
        jobReqMap[job.getId()] = &job.getRequirements();
    }

    // Accumulate allocated resource-times.
    double allocCpu = 0.0, allocRam = 0.0, allocStorage = 0.0, allocGpu = 0.0;

    for (const auto& [jobId, iv] : intervals) {
        const int dur = iv.finishTick - iv.startTick;
        if (dur <= 0) continue;

        auto reqIt = jobReqMap.find(jobId);
        if (reqIt == jobReqMap.end()) continue;

        const ResourceRequirements& req = *reqIt->second;
        allocCpu     += req.cpu     * static_cast<double>(dur);
        allocRam     += req.ram     * static_cast<double>(dur);
        allocStorage += req.storage * static_cast<double>(dur);
        allocGpu     += req.gpu     * static_cast<double>(dur);
    }

    // Accumulate total available resource-times across all servers.
    double totalCpu = 0.0, totalRam = 0.0, totalStorage = 0.0, totalGpu = 0.0;

    if (elapsed > 0) {
        for (const auto& sptr : dataCenter.getServers()) {
            const ResourceRequirements& cap = sptr->getCapacity();
            totalCpu     += cap.cpu     * static_cast<double>(elapsed);
            totalRam     += cap.ram     * static_cast<double>(elapsed);
            totalStorage += cap.storage * static_cast<double>(elapsed);
            totalGpu     += cap.gpu     * static_cast<double>(elapsed);
        }
    }
    // When elapsed == 0, all totals remain 0 → all utilizations remain nullopt.

    if (totalCpu     > 0.0) m.cpuUtilization     = allocCpu     / totalCpu;
    if (totalRam     > 0.0) m.ramUtilization     = allocRam     / totalRam;
    if (totalStorage > 0.0) m.storageUtilization = allocStorage / totalStorage;
    if (totalGpu     > 0.0) m.gpuUtilization     = allocGpu     / totalGpu;

    return m;
}
