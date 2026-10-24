#pragma once

#include "Job.h"
#include "ResourceRequirements.h"
#include "StrategyComparison.h"   // JobSpec, ServerSpec, ServerKind

#include <cstdint>
#include <random>
#include <string>
#include <vector>

/// Configuration for the workload generator.
///
/// All values are validated when passed to WorkloadGenerator.
/// Default values are chosen to produce a realistic, heterogeneous workload
/// that is neither trivially schedulable nor universally impossible.
struct WorkloadConfig {
    int    jobCount    = 20;       ///< Number of jobs to generate  (>= 1).
    int    serverCount = 5;        ///< Number of servers to generate (>= 1).
    uint32_t seed      = 42;       ///< RNG seed — same seed + config → same workload.
    int    maxArrival  = 50;       ///< Upper bound on arrival tick  (>= 0).
    int    maxDuration = 10;       ///< Upper bound on job duration  (>= 1).
};

/// Generates reproducible synthetic workloads for Orchestrix simulations.
///
/// Design decisions
/// ────────────────
/// • Uses std::mt19937 seeded from WorkloadConfig::seed so identical configs
///   always produce identical workloads (simulation-rules.md §11).
/// • Generates heterogeneous servers: ComputeServers (≥ 60 %), GPUServers
///   (≈ 20 %) and StorageServers (≈ 20 %) with randomised capacities that
///   keep within the documented bounds.
/// • Generates a mix of CPU-bound, GPU-bound, storage-bound and hybrid jobs
///   so that not every job lands on the same server type.
/// • Resource requirements are bounded so that neither all jobs fit trivially
///   (too small) nor none can ever fit (too large).
/// • Workload separation: generate() populates JobSpecs and ServerSpecs that
///   are value-copyable and reusable across strategy-comparison runs without
///   any shared mutable state.
///
/// Bounds
/// ──────
/// Servers
///   ComputeServer:  cpu [8, 32], ram [16, 128] GB, storage [200, 1000] GB, gpu 0
///   GPUServer:      cpu [8, 16], ram [16,  64] GB, storage [ 50,  200] GB, gpu [2, 8]
///   StorageServer:  cpu [4,  8], ram [ 8,  32] GB, storage [500, 5000] GB, gpu 0
///
/// Jobs
///   cpu     [0, 8]   (0 → CPU not required)
///   ram     [0, 32]  GB
///   storage [0, 500] GB
///   gpu     [0, 2]   (0 → GPU not required)
///   At least one resource dimension is > 0 (enforced by construction).
///   arrival  [0, maxArrival]
///   duration [1, maxDuration]
///   priority [0, 9]
class WorkloadGenerator {
public:
    /// Constructs the generator and validates the config.
    ///
    /// Throws std::invalid_argument when:
    ///   - jobCount < 1
    ///   - serverCount < 1
    ///   - maxDuration < 1
    explicit WorkloadGenerator(WorkloadConfig config);

    /// Generates ServerSpecs from the config.
    ///
    /// The same seed and config always produce the same servers, independent
    /// of how many times generate() has been called.
    [[nodiscard]] std::vector<ServerSpec> generateServers() const;

    /// Generates JobSpecs from the config.
    ///
    /// The same seed and config always produce the same jobs, independent
    /// of how many times generateJobs() has been called.
    [[nodiscard]] std::vector<JobSpec> generateJobs() const;

    /// Returns the config used to construct this generator.
    [[nodiscard]] const WorkloadConfig& config() const noexcept;

private:
    WorkloadConfig m_config;

    /// Seed the RNG from the config seed, offset by a domain tag, so that
    /// server generation and job generation never share the same stream.
    [[nodiscard]] std::mt19937 makeRng(uint32_t domainTag) const;

    /// Pick a random integer in [lo, hi] using the given rng.
    template<typename IntType>
    static IntType randInt(std::mt19937& rng, IntType lo, IntType hi);

    /// Pick a random double in [lo, hi] using the given rng.
    static double randDouble(std::mt19937& rng, double lo, double hi);
};
