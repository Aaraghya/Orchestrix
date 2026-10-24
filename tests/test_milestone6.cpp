/// test_milestone6.cpp — Final integration, audit and edge-case tests.
///
/// Focus areas:
///  1. Maximum-tick termination: elapsed ticks and metric accounting.
///  2. Manually calculated resource-time utilization.
///  3. Waiting-time and throughput definitions.
///  4. Documented strategy selection and deterministic tie-breaking.
///  5. Resource-accounting invariants over generated workloads.
///  6. Strategy comparison on a workload where strategies genuinely differ.

#include "AllocationStrategy.h"
#include "DataCenter.h"
#include "Job.h"
#include "MetricsCollector.h"
#include "ResourceRequirements.h"
#include "Scheduler.h"
#include "Server.h"
#include "SimulationEngine.h"
#include "SimulationEvent.h"
#include "SimulationMetrics.h"
#include "StrategyComparison.h"
#include "WorkloadGenerator.h"

#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// ─── Micro test harness (same style as the other milestone suites) ────────────

struct TestResult {
    std::string name;
    bool        passed;
    std::string message;
};

static std::vector<TestResult> g_results;
static bool        g_currentPassed = true;
static std::string g_currentMessage;

static void runTest(const std::string& name, const std::function<void()>& body) {
    g_currentPassed = true;
    g_currentMessage.clear();
    try { body(); }
    catch (const std::exception& ex) {
        g_currentPassed = false;
        g_currentMessage += std::string("\n  Unexpected exception: ") + ex.what();
    } catch (...) {
        g_currentPassed = false;
        g_currentMessage += "\n  Unexpected unknown exception.";
    }
    g_results.push_back({name, g_currentPassed, g_currentMessage});
}

#define TEST(name, ...) runTest((name), [&]() { __VA_ARGS__ })

#define EXPECT_TRUE(expr) \
    do { if (!(expr)) { g_currentPassed = false; \
         g_currentMessage += "\n  EXPECT_TRUE failed: " #expr; } } while (false)

#define EXPECT_FALSE(expr) \
    do { if ((expr)) { g_currentPassed = false; \
         g_currentMessage += "\n  EXPECT_FALSE failed: " #expr; } } while (false)

#define EXPECT_EQ(a, b) \
    do { if ((a) != (b)) { g_currentPassed = false; \
         g_currentMessage += "\n  EXPECT_EQ failed: " #a " != " #b; } } while (false)

#define EXPECT_NEAR(a, b, tol) \
    do { \
        double _a = static_cast<double>(a); \
        double _b = static_cast<double>(b); \
        double _t = static_cast<double>(tol); \
        if (std::abs(_a - _b) > _t) { \
            g_currentPassed = false; \
            g_currentMessage += "\n  EXPECT_NEAR failed: |" #a " - " #b "| > " #tol; \
        } \
    } while (false)

#define EXPECT_HAS_VALUE(opt) \
    do { if (!(opt).has_value()) { g_currentPassed = false; \
         g_currentMessage += "\n  EXPECT_HAS_VALUE failed: " #opt " is nullopt"; } } while (false)

#define EXPECT_NO_VALUE(opt) \
    do { if ((opt).has_value()) { g_currentPassed = false; \
         g_currentMessage += "\n  EXPECT_NO_VALUE failed: " #opt " has a value"; } } while (false)

static const double EPS = 1e-9;

static Job makeJob(const std::string& id,
                   const ResourceRequirements& req,
                   int arrivalTick = 0,
                   int duration    = 1)
{
    return Job{id, req, 0, arrivalTick, duration};
}

static SimulationMetrics runAndCollect(DataCenter& dc,
                                       Scheduler& sched,
                                       std::vector<Job> jobs,
                                       int maxTick = 100'000)
{
    SimulationEngine engine{sched, dc, maxTick};
    engine.setWorkload(std::move(jobs));
    engine.run();
    return MetricsCollector::collect(engine, dc, sched.strategyName());
}

static DataCenter buildDataCenter(const std::vector<ServerSpec>& specs) {
    DataCenter dc;
    for (const auto& s : specs) {
        switch (s.kind) {
            case ServerKind::Compute:
                dc.addServer(std::make_unique<ComputeServer>(s.id, s.capacity));
                break;
            case ServerKind::GPU:
                dc.addServer(std::make_unique<GPUServer>(s.id, s.capacity));
                break;
            case ServerKind::Storage:
                dc.addServer(std::make_unique<StorageServer>(s.id, s.capacity));
                break;
        }
    }
    return dc;
}

static std::vector<Job> buildJobs(const std::vector<JobSpec>& specs) {
    std::vector<Job> jobs;
    jobs.reserve(specs.size());
    for (const auto& s : specs) {
        jobs.emplace_back(s.id, s.requirements, s.priority, s.arrivalTick, s.duration);
    }
    return jobs;
}

// ─── 1. Maximum-tick termination ─────────────────────────────────────────────

static void test_MaxTickElapsed() {

    TEST("MaxTick: currentTick is the last processed tick, not maxTick + 1", {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{2.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc, /*maxTick=*/2};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("long", ResourceRequirements{1.0, 0.0, 0.0, 0.0}, 0, 10));
        engine.setWorkload(std::move(jobs));
        engine.run();

        EXPECT_TRUE(engine.hitMaxTick());
        EXPECT_EQ(engine.currentTick(), 2);

        SimulationMetrics m = MetricsCollector::collect(engine, dc, "FirstFit");
        EXPECT_EQ(m.elapsedTicks, 2);
        EXPECT_TRUE(m.hitMaxTick);
        EXPECT_EQ(m.stillActive, 1);
        EXPECT_EQ(m.completed, 0);
    });

    TEST("MaxTick: SIM_MAX_TICK event is logged at the max tick", {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{2.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc, /*maxTick=*/4};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("long", ResourceRequirements{1.0, 0.0, 0.0, 0.0}, 0, 20));
        engine.setWorkload(std::move(jobs));
        engine.run();

        const SimulationEvent* maxEvent = nullptr;
        for (const auto& ev : engine.getEvents()) {
            if (ev.type == EventType::SIM_MAX_TICK) maxEvent = &ev;
        }
        EXPECT_TRUE(maxEvent != nullptr);
        if (maxEvent) EXPECT_EQ(maxEvent->tick, 4);
    });

    TEST("MaxTick: maxTick = 0 yields zero elapsed and undefined time-based metrics", {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 5));
        SimulationMetrics m = runAndCollect(dc, sched, std::move(jobs), /*maxTick=*/0);

        EXPECT_TRUE(m.hitMaxTick);
        EXPECT_EQ(m.elapsedTicks, 0);
        EXPECT_NO_VALUE(m.throughput);
        EXPECT_NO_VALUE(m.cpuUtilization);
        EXPECT_EQ(m.stillActive, 1);
        // The job did start at tick 0 before the limit was reached.
        EXPECT_HAS_VALUE(m.avgWaitingTime);
        EXPECT_NEAR(m.avgWaitingTime.value(), 0.0, EPS);
    });

    TEST("MaxTick: job that has not arrived stays PENDING and is stillActive", {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("late", ResourceRequirements{1.0, 0.0, 0.0, 0.0}, 5, 1));
        SimulationMetrics m = runAndCollect(dc, sched, std::move(jobs), /*maxTick=*/0);

        EXPECT_TRUE(m.hitMaxTick);
        EXPECT_EQ(m.elapsedTicks, 0);
        EXPECT_EQ(m.stillActive, 1);
        EXPECT_NO_VALUE(m.avgWaitingTime);
        EXPECT_NO_VALUE(m.throughput);
    });

    TEST("MaxTick: normal completion still reports the last completion tick", {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc, /*maxTick=*/100};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j", ResourceRequirements{1.0, 0.0, 0.0, 0.0}, 0, 3));
        engine.setWorkload(std::move(jobs));
        engine.run();

        EXPECT_FALSE(engine.hitMaxTick());
        EXPECT_EQ(engine.currentTick(), 3);
    });
}

// ─── 2. Manually calculated utilization ──────────────────────────────────────

static void test_ManualUtilization() {

    TEST("Utilization: hand-calculated two-job CPU/RAM scenario", {
        // Server: 8 CPU, 16 GB RAM.
        // A: 2 CPU, 4 GB, arrives 0, duration 3  -> occupies [0,3)
        // B: 4 CPU, 8 GB, arrives 1, duration 2  -> occupies [1,3)
        // elapsed = 3.
        // allocated CPU-time = 2*3 + 4*2 = 14; total = 8*3 = 24 -> 14/24
        // allocated RAM-time = 4*3 + 8*2 = 28; total = 16*3 = 48 -> 28/48
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{8.0, 16.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("A", ResourceRequirements{2.0, 4.0, 0.0, 0.0}, 0, 3));
        jobs.push_back(makeJob("B", ResourceRequirements{4.0, 8.0, 0.0, 0.0}, 1, 2));
        SimulationMetrics m = runAndCollect(dc, sched, std::move(jobs));

        EXPECT_EQ(m.completed, 2);
        EXPECT_EQ(m.elapsedTicks, 3);
        EXPECT_HAS_VALUE(m.cpuUtilization);
        EXPECT_NEAR(m.cpuUtilization.value(), 14.0 / 24.0, EPS);
        EXPECT_HAS_VALUE(m.ramUtilization);
        EXPECT_NEAR(m.ramUtilization.value(), 28.0 / 48.0, EPS);
        EXPECT_NO_VALUE(m.storageUtilization);
        EXPECT_NO_VALUE(m.gpuUtilization);

        // Throughput = 2 completed / 3 ticks.  Both jobs fit concurrently,
        // so neither waits: average waiting time = 0.
        EXPECT_HAS_VALUE(m.throughput);
        EXPECT_NEAR(m.throughput.value(), 2.0 / 3.0, EPS);
        EXPECT_HAS_VALUE(m.avgWaitingTime);
        EXPECT_NEAR(m.avgWaitingTime.value(), 0.0, EPS);
        EXPECT_EQ(m.waitingTimeSampleCount, 2);
    });

    TEST("Utilization: hand-calculated four-dimension scenario", {
        // Server: 4 CPU, 8 GB RAM, 100 GB storage, 2 GPU.
        // Job: 1 CPU, 2 GB RAM, 10 GB storage, 1 GPU, duration 4.
        // CPU  = 1*4 / (4*4)   = 4/16   = 0.25
        // RAM  = 2*4 / (8*4)   = 8/32   = 0.25
        // Stor = 10*4 / (100*4)= 40/400 = 0.10
        // GPU  = 1*4 / (2*4)   = 4/8    = 0.50
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 8.0, 100.0, 2.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("mixed",
            ResourceRequirements{1.0, 2.0, 10.0, 1.0}, 0, 4));
        SimulationMetrics m = runAndCollect(dc, sched, std::move(jobs));

        EXPECT_EQ(m.elapsedTicks, 4);
        EXPECT_HAS_VALUE(m.cpuUtilization);
        EXPECT_HAS_VALUE(m.ramUtilization);
        EXPECT_HAS_VALUE(m.storageUtilization);
        EXPECT_HAS_VALUE(m.gpuUtilization);
        EXPECT_NEAR(m.cpuUtilization.value(),     0.25, EPS);
        EXPECT_NEAR(m.ramUtilization.value(),     0.25, EPS);
        EXPECT_NEAR(m.storageUtilization.value(), 0.10, EPS);
        EXPECT_NEAR(m.gpuUtilization.value(),     0.50, EPS);
    });

    TEST("Utilization: idle time before a late arrival lowers utilization", {
        // Server 4 CPU.  Job 2 CPU, arrival 3, duration 2.
        // Resource-time = 2*2 = 4.  Total = 4*5 = 20.  Util = 0.20.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("late", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 3, 2));
        SimulationMetrics m = runAndCollect(dc, sched, std::move(jobs));

        EXPECT_EQ(m.elapsedTicks, 5);
        EXPECT_HAS_VALUE(m.cpuUtilization);
        EXPECT_NEAR(m.cpuUtilization.value(), 0.20, EPS);
        EXPECT_HAS_VALUE(m.throughput);
        EXPECT_NEAR(m.throughput.value(), 1.0 / 5.0, EPS);
    });
}

// ─── 3. Waiting time under contention ───────────────────────────────────────

static void test_WaitingTimeContention() {

    TEST("Waiting time: second job waits for the first to release resources", {
        // Server 2 CPU.
        // j1: 2 CPU, arrival 0, duration 3 -> starts 0, finishes 3.
        // j2: 2 CPU, arrival 1, duration 1 -> starts 3, finishes 4.
        // Waiting times: 0 and 2.  Average = 1.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{2.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j1", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 3));
        jobs.push_back(makeJob("j2", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 1, 1));
        SimulationMetrics m = runAndCollect(dc, sched, std::move(jobs));

        EXPECT_EQ(m.completed, 2);
        EXPECT_EQ(m.elapsedTicks, 4);
        EXPECT_EQ(m.waitingTimeSampleCount, 2);
        EXPECT_HAS_VALUE(m.avgWaitingTime);
        EXPECT_NEAR(m.avgWaitingTime.value(), 1.0, EPS);
    });

    TEST("Waiting time: never-started job is not averaged as zero", {
        // Server 1 CPU.  j1 occupies it for 5 ticks.
        // j2 is impossible (2 CPU > capacity 1) and is REJECTED.
        // Only j1 contributes to the average.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{1.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j1", ResourceRequirements{1.0, 0.0, 0.0, 0.0}, 0, 5));
        jobs.push_back(makeJob("j2", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 1));
        SimulationMetrics m = runAndCollect(dc, sched, std::move(jobs));

        EXPECT_EQ(m.completed, 1);
        EXPECT_EQ(m.rejected, 1);
        EXPECT_EQ(m.waitingTimeSampleCount, 1);
        EXPECT_HAS_VALUE(m.avgWaitingTime);
        EXPECT_NEAR(m.avgWaitingTime.value(), 0.0, EPS);
    });
}

// ─── 4. Strategy selection and tie-breaking ─────────────────────────────────

static void test_StrategySelectionRules() {

    TEST("Strategy: FirstFit, BestFit and LoadBalanced select per documented rules", {
        // srv-a: 10 of each resource; srv-b: 4 of each resource.
        // Job: 1 of each.
        // First Fit  -> srv-a (first feasible).
        // Best Fit   -> srv-b (tightest residual: 3/4*3 < 9/10*3).
        // LoadBalanced -> srv-a (lowest projected average utilization).
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("srv-a",
            ResourceRequirements{10.0, 10.0, 10.0, 0.0}));
        dc.addServer(std::make_unique<ComputeServer>("srv-b",
            ResourceRequirements{4.0, 4.0, 4.0, 0.0}));

        const ResourceRequirements req{1.0, 1.0, 1.0, 0.0};

        FirstFit ff;
        BestFit  bf;
        LoadBalanced lb;

        Server* ffChoice = ff.select(dc.getServers(), req);
        Server* bfChoice = bf.select(dc.getServers(), req);
        Server* lbChoice = lb.select(dc.getServers(), req);

        EXPECT_TRUE(ffChoice != nullptr);
        EXPECT_TRUE(bfChoice != nullptr);
        EXPECT_TRUE(lbChoice != nullptr);
        if (ffChoice) EXPECT_EQ(ffChoice->getId(), std::string("srv-a"));
        if (bfChoice) EXPECT_EQ(bfChoice->getId(), std::string("srv-b"));
        if (lbChoice) EXPECT_EQ(lbChoice->getId(), std::string("srv-a"));
    });

    TEST("Strategy: BestFit and LoadBalanced break ties by smaller server ID", {
        // Identical capacities; insertion order deliberately reversed.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("srv-z",
            ResourceRequirements{10.0, 10.0, 10.0, 0.0}));
        dc.addServer(std::make_unique<ComputeServer>("srv-a",
            ResourceRequirements{10.0, 10.0, 10.0, 0.0}));

        const ResourceRequirements req{1.0, 1.0, 1.0, 0.0};

        FirstFit ff;
        BestFit  bf;
        LoadBalanced lb;

        Server* ffChoice = ff.select(dc.getServers(), req);
        Server* bfChoice = bf.select(dc.getServers(), req);
        Server* lbChoice = lb.select(dc.getServers(), req);

        if (ffChoice) EXPECT_EQ(ffChoice->getId(), std::string("srv-z"));
        if (bfChoice) EXPECT_EQ(bfChoice->getId(), std::string("srv-a"));
        if (lbChoice) EXPECT_EQ(lbChoice->getId(), std::string("srv-a"));
    });

    TEST("Strategy: no feasible server returns nullptr", {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{1.0, 1.0, 1.0, 0.0}));
        const ResourceRequirements req{2.0, 0.0, 0.0, 0.0};

        FirstFit ff;
        BestFit  bf;
        LoadBalanced lb;
        EXPECT_TRUE(ff.select(dc.getServers(), req) == nullptr);
        EXPECT_TRUE(bf.select(dc.getServers(), req) == nullptr);
        EXPECT_TRUE(lb.select(dc.getServers(), req) == nullptr);
    });
}

// ─── 5. Strategy comparison difference audit ────────────────────────────────

static void test_StrategyComparisonDivergence() {

    TEST("Comparison: seed 84 produces different waiting times per strategy", {
        // Default CLI workload (20 jobs, 5 servers, arrival <= 50, duration <= 10).
        // On this workload all strategies complete all 20 jobs, but their
        // placement decisions create different waiting times.
        WorkloadConfig cfg;
        cfg.jobCount    = 20;
        cfg.serverCount = 5;
        cfg.seed        = 84;

        WorkloadGenerator gen{cfg};
        StrategyComparison cmp{gen.generateServers(), gen.generateJobs()};
        std::vector<SimulationMetrics> r = cmp.run();

        EXPECT_EQ(r.size(), std::size_t{3});
        if (r.size() == 3) {
            EXPECT_EQ(r[0].strategyName, std::string("FirstFit"));
            EXPECT_EQ(r[1].strategyName, std::string("BestFit"));
            EXPECT_EQ(r[2].strategyName, std::string("LoadBalanced"));
            for (const auto& m : r) {
                EXPECT_EQ(m.totalSubmitted, 20);
                EXPECT_EQ(m.completed, 20);
                EXPECT_EQ(m.rejected, 0);
                EXPECT_HAS_VALUE(m.avgWaitingTime);
            }
            EXPECT_TRUE(r[0].avgWaitingTime != r[1].avgWaitingTime);
            EXPECT_TRUE(r[1].avgWaitingTime != r[2].avgWaitingTime);
        }
    });

    TEST("Comparison: repeated run() is deterministic and independent", {
        WorkloadConfig cfg;
        cfg.jobCount    = 20;
        cfg.serverCount = 5;
        cfg.seed        = 84;

        WorkloadGenerator gen{cfg};
        StrategyComparison cmp{gen.generateServers(), gen.generateJobs()};
        auto first  = cmp.run();
        auto second = cmp.run();

        EXPECT_EQ(first.size(), second.size());
        for (std::size_t i = 0; i < first.size() && i < second.size(); ++i) {
            EXPECT_EQ(first[i].completed,       second[i].completed);
            EXPECT_EQ(first[i].rejected,        second[i].rejected);
            EXPECT_EQ(first[i].elapsedTicks,    second[i].elapsedTicks);
            EXPECT_EQ(first[i].avgWaitingTime,  second[i].avgWaitingTime);
            EXPECT_EQ(first[i].cpuUtilization,  second[i].cpuUtilization);
        }
    });
}

// ─── 6. Resource safety over generated workloads ────────────────────────────

static void test_ResourceSafetySweep() {

    TEST("Safety: terminal runs leave every server fully released", {
        const uint32_t seeds[] = {1u, 42u, 84u, 99u};

        for (uint32_t seed : seeds) {
            WorkloadConfig cfg;
            cfg.jobCount    = 200;
            cfg.serverCount = 8;
            cfg.seed        = seed;

            WorkloadGenerator gen{cfg};
            auto serverSpecs = gen.generateServers();
            auto jobSpecs    = gen.generateJobs();

            DataCenter dc = buildDataCenter(serverSpecs);
            FirstFit ff;
            Scheduler sched{ff, dc};
            SimulationEngine engine{sched, dc};
            engine.setWorkload(buildJobs(jobSpecs));
            engine.run();

            EXPECT_FALSE(engine.hitMaxTick());

            // Every job must be terminal after a normal run.
            for (const auto& job : engine.getJobs()) {
                const JobState s = job.getState();
                EXPECT_TRUE(s == JobState::COMPLETED || s == JobState::REJECTED);
            }

            // No server may retain allocations once the run is complete.
            for (const auto& sptr : dc.getServers()) {
                const ResourceRequirements allocated = sptr->getAllocated();
                const ResourceRequirements available = sptr->getAvailable();
                EXPECT_NEAR(allocated.cpu,     0.0, EPS);
                EXPECT_NEAR(allocated.ram,     0.0, EPS);
                EXPECT_NEAR(allocated.storage, 0.0, EPS);
                EXPECT_NEAR(allocated.gpu,     0.0, EPS);
                EXPECT_TRUE(available.cpu     >= 0.0);
                EXPECT_TRUE(available.ram     >= 0.0);
                EXPECT_TRUE(available.storage >= 0.0);
                EXPECT_TRUE(available.gpu     >= 0.0);
            }
        }
    });

    TEST("Safety: generated workload metrics stay within valid ranges", {
        const uint32_t seeds[] = {1u, 42u, 84u, 99u};

        for (uint32_t seed : seeds) {
            WorkloadConfig cfg;
            cfg.jobCount    = 200;
            cfg.serverCount = 8;
            cfg.seed        = seed;

            WorkloadGenerator gen{cfg};
            auto serverSpecs = gen.generateServers();
            auto jobSpecs    = gen.generateJobs();

            DataCenter dc = buildDataCenter(serverSpecs);
            BestFit bf;
            Scheduler sched{bf, dc};
            SimulationMetrics m = runAndCollect(dc, sched, buildJobs(jobSpecs));

            EXPECT_FALSE(m.hitMaxTick);
            EXPECT_EQ(m.totalSubmitted, 200);
            EXPECT_EQ(m.completed + m.rejected + m.cancelled + m.failed + m.stillActive,
                      m.totalSubmitted);

            const std::optional<double>* utils[] = {
                &m.cpuUtilization, &m.ramUtilization,
                &m.storageUtilization, &m.gpuUtilization
            };
            for (const auto* u : utils) {
                if (u->has_value()) {
                    EXPECT_TRUE(u->value() >= 0.0);
                    EXPECT_TRUE(u->value() <= 1.0 + EPS);
                }
            }
        }
    });

    TEST("Safety: reservation and release keep capacity invariant", {
        ComputeServer server{"cs", ResourceRequirements{4.0, 8.0, 10.0, 1.0}};
        const ResourceRequirements a{2.0, 4.0, 5.0, 1.0};
        const ResourceRequirements b{2.0, 4.0, 5.0, 0.0};

        server.reserveResources(a);
        EXPECT_NEAR(server.getAvailable().cpu, 2.0, EPS);
        EXPECT_NEAR(server.getAvailable().gpu, 0.0, EPS);

        // A second CPU-only-too-large reservation must fail atomically.
        bool threw = false;
        try {
            server.reserveResources(ResourceRequirements{3.0, 1.0, 1.0, 0.0});
        } catch (const std::runtime_error&) {
            threw = true;
        }
        EXPECT_TRUE(threw);
        EXPECT_NEAR(server.getAvailable().cpu, 2.0, EPS);
        EXPECT_NEAR(server.getAllocated().cpu, 2.0, EPS);

        server.reserveResources(b);
        // Releasing the reservation in full restores the original capacity.
        server.releaseResources(a);
        server.releaseResources(b);
        EXPECT_NEAR(server.getAvailable().cpu,     4.0, EPS);
        EXPECT_NEAR(server.getAvailable().ram,     8.0, EPS);
        EXPECT_NEAR(server.getAvailable().storage, 10.0, EPS);
        EXPECT_NEAR(server.getAvailable().gpu,     1.0, EPS);
    });
}

// ─── main ────────────────────────────────────────────────────────────────────

int main() {
    test_MaxTickElapsed();
    test_ManualUtilization();
    test_WaitingTimeContention();
    test_StrategySelectionRules();
    test_StrategyComparisonDivergence();
    test_ResourceSafetySweep();

    int passed = 0;
    int failed = 0;

    for (const auto& r : g_results) {
        if (r.passed) {
            ++passed;
            std::cout << "  [PASS] " << r.name << "\n";
        } else {
            ++failed;
            std::cout << "  [FAIL] " << r.name << r.message << "\n";
        }
    }

    std::cout << "\n--- " << passed << " passed, " << failed << " failed ---\n";
    return failed == 0 ? 0 : 1;
}
