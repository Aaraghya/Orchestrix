/// main.cpp — Orchestrix Milestone 5 CLI
///
/// Provides a user-friendly interactive menu that:
///   1. Allows configuring job count and random seed.
///   2. Runs simulations with First Fit, Best Fit, or Load Balanced strategies.
///   3. Compares all three strategies side-by-side.
///
/// All simulation logic is delegated to the existing library classes.
/// The CLI only handles user interaction and output formatting.

#include "AllocationStrategy.h"
#include "DataCenter.h"
#include "Job.h"
#include "MetricsCollector.h"
#include "ReportGenerator.h"
#include "ResourceRequirements.h"
#include "Scheduler.h"
#include "Server.h"
#include "SimulationEngine.h"
#include "SimulationMetrics.h"
#include "StrategyComparison.h"
#include "WorkloadGenerator.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

// ─── Banner and helpers ───────────────────────────────────────────────────────

static const std::string LINE_WIDE(60, '=');
static const std::string LINE_THIN(60, '-');

static void printBanner() {
    std::cout << "\n" << LINE_WIDE << "\n";
    std::cout << "   ___           _               _        _\n";
    std::cout << "  / _ \\ _ __ ___| |__   ___  ___| |_ _ __(_)_  __\n";
    std::cout << " | | | | '__/ __| '_ \\ / _ \\/ __| __| '__| \\ \\/ /\n";
    std::cout << " | |_| | | | (__| | | |  __/\\__ \\ |_| |  | |>  <\n";
    std::cout << "  \\___/|_|  \\___|_| |_|\\___||___/\\__|_|  |_/_/\\_\\\n";
    std::cout << "\n";
    std::cout << "  Discrete-Event Simulator for Adaptive Data-Center\n";
    std::cout << "  Resource Allocation  |  C++17  |  v1.0\n";
    std::cout << LINE_WIDE << "\n\n";
}


static void printMenu() {
    std::cout << LINE_THIN << "\n";
    std::cout << "  MAIN MENU\n";
    std::cout << LINE_THIN << "\n";
    std::cout << "  1. Run simulation - First Fit\n";
    std::cout << "  2. Run simulation - Best Fit\n";
    std::cout << "  3. Run simulation - Load Balanced\n";
    std::cout << "  4. Compare all three strategies\n";
    std::cout << "  5. Configure workload (job count / seed)\n";
    std::cout << "  6. Exit\n";
    std::cout << LINE_THIN << "\n";
    std::cout << "  Choice: ";
}

/// Read an integer from stdin, returning false and leaving \p out unchanged if
/// parsing fails or the value is out of [lo, hi].
static bool readInt(const std::string& prompt, int lo, int hi, int& out) {
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) return false;
    try {
        std::size_t pos = 0;
        int v = std::stoi(line, &pos);
        // Ensure the whole token was consumed (not "12abc").
        if (pos != line.size()) return false;
        if (v < lo || v > hi)   return false;
        out = v;
        return true;
    } catch (...) {
        return false;
    }
}

/// Read an unsigned 32-bit integer (seed) from stdin.
static bool readUint32(const std::string& prompt, uint32_t& out) {
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) return false;
    try {
        std::size_t pos = 0;
        unsigned long v = std::stoul(line, &pos);
        if (pos != line.size()) return false;
        if (v > std::numeric_limits<uint32_t>::max()) return false;
        out = static_cast<uint32_t>(v);
        return true;
    } catch (...) {
        return false;
    }
}

// ─── Config display ───────────────────────────────────────────────────────────

static void printConfig(const WorkloadConfig& cfg) {
    std::cout << "\n  Current workload settings:\n";
    std::cout << "    Jobs       : " << cfg.jobCount    << "\n";
    std::cout << "    Servers    : " << cfg.serverCount << "\n";
    std::cout << "    Seed       : " << cfg.seed        << "\n";
    std::cout << "    Max arrival: " << cfg.maxArrival  << " tick(s)\n";
    std::cout << "    Max dur.   : " << cfg.maxDuration << " tick(s)\n";
}

// ─── Single-strategy runner ────────────────────────────────────────────────────

static void runWithStrategy(AllocationStrategy& strategy,
                            const std::vector<ServerSpec>& serverSpecs,
                            const std::vector<JobSpec>&    jobSpecs,
                            const WorkloadConfig&           cfg,
                            int maxTick)
{
    // Build fresh DataCenter.
    DataCenter dc;
    for (const auto& spec : serverSpecs) {
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

    // Build fresh Job vector.
    std::vector<Job> jobs;
    jobs.reserve(jobSpecs.size());
    for (const auto& spec : jobSpecs) {
        jobs.emplace_back(spec.id, spec.requirements,
                          spec.priority, spec.arrivalTick, spec.duration);
    }

    // Run simulation.
    Scheduler        sched{strategy, dc};
    SimulationEngine engine{sched, dc, maxTick};
    engine.setWorkload(std::move(jobs));

    std::cout << "\n  Running " << strategy.name() << " on "
              << cfg.jobCount << " job(s) / " << cfg.serverCount
              << " server(s)  [seed=" << cfg.seed << "] ...\n";

    engine.run();

    // Collect and display.
    SimulationMetrics m = MetricsCollector::collect(engine, dc, strategy.name());
    std::cout << "\n" << ReportGenerator::generate(m) << "\n";
}

// ─── Comparison runner ─────────────────────────────────────────────────────────

static void runComparison(const std::vector<ServerSpec>& serverSpecs,
                          const std::vector<JobSpec>&    jobSpecs,
                          const WorkloadConfig&           cfg,
                          int maxTick)
{
    std::cout << "\n  Comparing all three strategies on "
              << cfg.jobCount << " job(s) / " << cfg.serverCount
              << " server(s)  [seed=" << cfg.seed << "] ...\n";

    StrategyComparison cmp{serverSpecs, jobSpecs, maxTick};
    auto results = cmp.run();

    std::cout << "\n" << ReportGenerator::generateComparison(results) << "\n";
}

// ─── Configure workload ────────────────────────────────────────────────────────

static void configureWorkload(WorkloadConfig& cfg) {
    std::cout << "\n" << LINE_THIN << "\n";
    std::cout << "  CONFIGURE WORKLOAD\n";
    std::cout << LINE_THIN << "\n";
    printConfig(cfg);
    std::cout << "\n";

    int tmp = 0;
    if (readInt("  New job count   [1-1000, Enter to keep " +
                    std::to_string(cfg.jobCount) + "]: ",
                1, 1000, tmp))
        cfg.jobCount = tmp;
    else
        std::cout << "  (Keeping " << cfg.jobCount << ")\n";

    if (readInt("  New server count [1-100, Enter to keep " +
                    std::to_string(cfg.serverCount) + "]: ",
                1, 100, tmp))
        cfg.serverCount = tmp;
    else
        std::cout << "  (Keeping " << cfg.serverCount << ")\n";

    uint32_t seed = cfg.seed;
    if (readUint32("  New seed        [0-4294967295, Enter to keep " +
                       std::to_string(cfg.seed) + "]: ",
                   seed))
        cfg.seed = seed;
    else
        std::cout << "  (Keeping seed " << cfg.seed << ")\n";

    std::cout << "\n  Updated workload settings:\n";
    printConfig(cfg);
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main() {
    printBanner();

    // Default workload configuration.
    WorkloadConfig cfg;     // uses sensible defaults from the struct
    constexpr int MAX_TICK = 100'000;

    bool running = true;
    while (running) {
        printConfig(cfg);
        std::cout << "\n";
        printMenu();

        std::string choice;
        if (!std::getline(std::cin, choice)) {
            // stdin reached EOF (e.g. piped input ended).  Treat it as a
            // request to exit instead of looping forever on an empty choice.
            std::cout << "\n  Input closed. Goodbye!\n\n";
            break;
        }

        // Trim leading/trailing whitespace.
        const auto trimmed = [](const std::string& s) -> std::string {
            std::size_t a = s.find_first_not_of(" \t\r\n");
            if (a == std::string::npos) return {};
            std::size_t b = s.find_last_not_of(" \t\r\n");
            return s.substr(a, b - a + 1);
        };
        const std::string ch = trimmed(choice);

        if (ch.empty()) {
            std::cout << "\n  Please enter a menu option (1-6).\n";
            continue;
        }

        // Generate fresh specs from the current config every time the user
        // starts a run, so the workload matches the displayed settings.
        auto generateWorkload = [&]() {
            WorkloadGenerator gen{cfg};
            return std::make_pair(gen.generateServers(), gen.generateJobs());
        };

        if (ch == "1" || ch == "2" || ch == "3") {
            auto [servers, jobs] = generateWorkload();
            if (ch == "1") { FirstFit    ff; runWithStrategy(ff, servers, jobs, cfg, MAX_TICK); }
            if (ch == "2") { BestFit     bf; runWithStrategy(bf, servers, jobs, cfg, MAX_TICK); }
            if (ch == "3") { LoadBalanced lb; runWithStrategy(lb, servers, jobs, cfg, MAX_TICK); }

        } else if (ch == "4") {
            auto [servers, jobs] = generateWorkload();
            runComparison(servers, jobs, cfg, MAX_TICK);

        } else if (ch == "5") {
            configureWorkload(cfg);

        } else if (ch == "6") {
            std::cout << "\n  Goodbye!\n\n";
            running = false;

        } else {
            std::cout << "\n  Invalid choice '" << ch
                      << "'. Please enter a number between 1 and 6.\n";
        }
    }

    return 0;
}
