#include "StrategyComparison.h"
#include "MetricsCollector.h"
#include "Scheduler.h"
#include "SimulationEngine.h"

#include <memory>
#include <stdexcept>

StrategyComparison::StrategyComparison(std::vector<ServerSpec> serverSpecs,
                                       std::vector<JobSpec>    jobSpecs,
                                       int maxTick)
    : m_serverSpecs{std::move(serverSpecs)}
    , m_jobSpecs{std::move(jobSpecs)}
    , m_maxTick{maxTick}
{}

std::vector<SimulationMetrics> StrategyComparison::run() {
    // Always compare in the fixed order: FirstFit, BestFit, LoadBalanced.
    FirstFit    ff;
    BestFit     bf;
    LoadBalanced lb;

    std::vector<SimulationMetrics> results;
    results.push_back(runOne(ff));
    results.push_back(runOne(bf));
    results.push_back(runOne(lb));
    return results;
}

// ── Private helpers ───────────────────────────────────────────────────────────

DataCenter StrategyComparison::buildDataCenter() const {
    DataCenter dc;
    for (const auto& spec : m_serverSpecs) {
        switch (spec.kind) {
            case ServerKind::Compute:
                dc.addServer(std::make_unique<ComputeServer>(spec.id, spec.capacity));
                break;
            case ServerKind::GPU:
                dc.addServer(std::make_unique<GPUServer>(spec.id, spec.capacity));
                break;
            case ServerKind::Storage:
                dc.addServer(std::make_unique<StorageServer>(spec.id, spec.capacity));
                break;
        }
    }
    return dc;
}

std::vector<Job> StrategyComparison::buildJobs() const {
    std::vector<Job> jobs;
    jobs.reserve(m_jobSpecs.size());
    for (const auto& spec : m_jobSpecs) {
        jobs.emplace_back(spec.id, spec.requirements,
                          spec.priority, spec.arrivalTick, spec.duration);
    }
    return jobs;
}

SimulationMetrics StrategyComparison::runOne(AllocationStrategy& strategy) const {
    // Each call to runOne() produces:
    //   - A fresh DataCenter (from m_serverSpecs)
    //   - A fresh vector<Job>  (from m_jobSpecs)
    //   - A fresh Scheduler    (holds strategy& and dc&)
    //   - A fresh engine       (owns the jobs)
    // Nothing from a previous run leaks into this one.

    DataCenter       dc     = buildDataCenter();
    Scheduler        sched{strategy, dc};
    SimulationEngine engine{sched, dc, m_maxTick};

    engine.setWorkload(buildJobs());
    engine.run();

    return MetricsCollector::collect(engine, dc, strategy.name());
}
