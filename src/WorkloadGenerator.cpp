#include "WorkloadGenerator.h"

#include <random>
#include <stdexcept>
#include <string>
#include <vector>

// ── Constructor ───────────────────────────────────────────────────────────────

WorkloadGenerator::WorkloadGenerator(WorkloadConfig config)
    : m_config{config}
{
    if (config.jobCount < 1)
        throw std::invalid_argument("WorkloadConfig: jobCount must be >= 1");
    if (config.serverCount < 1)
        throw std::invalid_argument("WorkloadConfig: serverCount must be >= 1");
    if (config.maxDuration < 1)
        throw std::invalid_argument("WorkloadConfig: maxDuration must be >= 1");
    if (config.maxArrival < 0)
        throw std::invalid_argument("WorkloadConfig: maxArrival must be >= 0");
}

// ── Accessors ─────────────────────────────────────────────────────────────────

const WorkloadConfig& WorkloadGenerator::config() const noexcept {
    return m_config;
}

// ── Private helpers ───────────────────────────────────────────────────────────

std::mt19937 WorkloadGenerator::makeRng(uint32_t domainTag) const {
    // XOR the seed with a domain tag so server and job generators never
    // share the same stream even if the user-supplied seed is 0.
    std::mt19937 rng(static_cast<uint32_t>(m_config.seed) ^ domainTag);
    return rng;
}

template<typename IntType>
IntType WorkloadGenerator::randInt(std::mt19937& rng, IntType lo, IntType hi) {
    std::uniform_int_distribution<IntType> dist(lo, hi);
    return dist(rng);
}

double WorkloadGenerator::randDouble(std::mt19937& rng, double lo, double hi) {
    std::uniform_real_distribution<double> dist(lo, hi);
    return dist(rng);
}

// ── Server generation ─────────────────────────────────────────────────────────

std::vector<ServerSpec> WorkloadGenerator::generateServers() const {
    // Domain tag 0xA1B2C3D4 separates the server RNG stream from the job stream.
    std::mt19937 rng = makeRng(0xA1B2C3D4u);

    std::vector<ServerSpec> specs;
    specs.reserve(static_cast<std::size_t>(m_config.serverCount));

    for (int i = 0; i < m_config.serverCount; ++i) {
        const std::string id = "srv-" + std::to_string(i);

        // Distribution: ~60 % Compute, ~20 % GPU, ~20 % Storage.
        const int roll = randInt(rng, 0, 9);  // [0,9]

        if (roll <= 5) {
            // ComputeServer: cpu [8,32], ram [16,128] GB, storage [200,1000] GB, gpu 0
            double cpu     = static_cast<double>(randInt(rng, 8, 32));
            double ram     = static_cast<double>(randInt(rng, 16, 128));
            double storage = static_cast<double>(randInt(rng, 200, 1000));
            specs.push_back({ServerKind::Compute, id,
                             ResourceRequirements{cpu, ram, storage, 0.0}});

        } else if (roll <= 7) {
            // GPUServer: cpu [8,16], ram [16,64] GB, storage [50,200] GB, gpu [2,8]
            double cpu     = static_cast<double>(randInt(rng, 8, 16));
            double ram     = static_cast<double>(randInt(rng, 16, 64));
            double storage = static_cast<double>(randInt(rng, 50, 200));
            double gpu     = static_cast<double>(randInt(rng, 2, 8));
            specs.push_back({ServerKind::GPU, id,
                             ResourceRequirements{cpu, ram, storage, gpu}});

        } else {
            // StorageServer: cpu [4,8], ram [8,32] GB, storage [500,5000] GB, gpu 0
            double cpu     = static_cast<double>(randInt(rng, 4, 8));
            double ram     = static_cast<double>(randInt(rng, 8, 32));
            double storage = static_cast<double>(randInt(rng, 500, 5000));
            specs.push_back({ServerKind::Storage, id,
                             ResourceRequirements{cpu, ram, storage, 0.0}});
        }
    }
    return specs;
}

// ── Job generation ────────────────────────────────────────────────────────────

std::vector<JobSpec> WorkloadGenerator::generateJobs() const {
    // Domain tag 0xE5F6A7B8 separates the job RNG stream from the server stream.
    std::mt19937 rng = makeRng(0xE5F6A7B8u);

    std::vector<JobSpec> specs;
    specs.reserve(static_cast<std::size_t>(m_config.jobCount));

    for (int i = 0; i < m_config.jobCount; ++i) {
        const std::string id = "job-" + std::to_string(i);

        // Job type selector (10 equally weighted bins).
        const int jtype = randInt(rng, 0, 9);

        double cpu = 0.0, ram = 0.0, storage = 0.0, gpu = 0.0;

        if (jtype <= 2) {
            // CPU-bound: cpu [2,8], ram [4,32] GB
            cpu = static_cast<double>(randInt(rng, 2, 8));
            ram = static_cast<double>(randInt(rng, 4, 32));

        } else if (jtype <= 4) {
            // Storage-bound: cpu [1,4], ram [2,8] GB, storage [50,500] GB
            cpu     = static_cast<double>(randInt(rng, 1, 4));
            ram     = static_cast<double>(randInt(rng, 2, 8));
            storage = static_cast<double>(randInt(rng, 50, 500));

        } else if (jtype <= 6) {
            // GPU-bound: cpu [2,4], ram [4,16] GB, gpu [1,2]
            cpu = static_cast<double>(randInt(rng, 2, 4));
            ram = static_cast<double>(randInt(rng, 4, 16));
            gpu = static_cast<double>(randInt(rng, 1, 2));

        } else {
            // Hybrid: all four resources
            cpu     = static_cast<double>(randInt(rng, 1, 6));
            ram     = static_cast<double>(randInt(rng, 4, 32));
            storage = static_cast<double>(randInt(rng, 20, 200));
            gpu     = static_cast<double>(randInt(rng, 0, 2));
            // Ensure at least one resource > 0 (cpu is always >= 1 here).
        }

        const int arrival  = (m_config.maxArrival > 0)
                                 ? randInt(rng, 0, m_config.maxArrival)
                                 : 0;
        const int duration = randInt(rng, 1, m_config.maxDuration);
        const int priority = randInt(rng, 0, 9);

        specs.push_back({id,
                         ResourceRequirements{cpu, ram, storage, gpu},
                         priority,
                         arrival,
                         duration});
    }
    return specs;
}
