#pragma once

#include "AllocationStrategy.h"
#include "DataCenter.h"
#include "Job.h"
#include "ResourceRequirements.h"
#include "Server.h"
#include "SimulationMetrics.h"

#include <memory>
#include <string>
#include <vector>

/// Specifies a job for use in strategy-comparison runs.
///
/// Because comparison runs must each receive fresh Job objects (simulation-rules.md §11),
/// jobs are described by their construction parameters rather than as pre-built objects.
struct JobSpec {
    std::string          id;
    ResourceRequirements requirements;
    int                  priority    = 0;
    int                  arrivalTick = 0;
    int                  duration    = 1;
};

/// Server type tag used when reconstructing the DataCenter for each run.
enum class ServerKind { Compute, GPU, Storage };

/// Specifies a server for use in strategy-comparison runs.
struct ServerSpec {
    ServerKind           kind;
    std::string          id;
    ResourceRequirements capacity;
};

/// Runs all three allocation strategies on an identical workload and returns
/// their metrics side-by-side.
///
/// Fresh-state guarantee (simulation-rules.md §11)
/// ────────────────────────────────────────────────
/// For every strategy run, StrategyComparison:
///  1. Builds a new DataCenter from the provided ServerSpecs.
///  2. Constructs fresh Job objects from the provided JobSpecs.
///  3. Creates a new Scheduler and SimulationEngine.
///  4. Runs the engine and collects metrics.
///
/// No server state, job state, or engine state is shared between runs.
///
/// Comparison order: [FirstFit, BestFit, LoadBalanced].
class StrategyComparison {
public:
    /// \param serverSpecs  Server configuration applied identically to each run.
    /// \param jobSpecs     Workload applied identically to each run.
    /// \param maxTick      Simulation tick limit (same for every run).
    StrategyComparison(std::vector<ServerSpec> serverSpecs,
                       std::vector<JobSpec>    jobSpecs,
                       int maxTick = 100'000);

    /// Runs all three strategies and returns one SimulationMetrics per strategy.
    /// Result order is always [FirstFit, BestFit, LoadBalanced].
    [[nodiscard]] std::vector<SimulationMetrics> run();

private:
    std::vector<ServerSpec> m_serverSpecs;
    std::vector<JobSpec>    m_jobSpecs;
    int                     m_maxTick;

    /// Builds a fresh DataCenter from m_serverSpecs.
    [[nodiscard]] DataCenter buildDataCenter() const;

    /// Builds a fresh job vector from m_jobSpecs.
    [[nodiscard]] std::vector<Job> buildJobs() const;

    /// Executes one strategy and returns its metrics.
    [[nodiscard]] SimulationMetrics runOne(AllocationStrategy& strategy) const;
};
