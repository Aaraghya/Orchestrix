/// test_milestone5.cpp — Automated tests for Milestone 5: CLI, Workload
///                        Generation & Strategy Comparison
///
/// Uses the same micro-harness as previous milestones.
///
/// Coverage:
///  1.  WorkloadGenerator: valid config produces valid servers.
///  2.  WorkloadGenerator: valid config produces valid jobs.
///  3.  WorkloadGenerator: identical seed+config → identical servers.
///  4.  WorkloadGenerator: identical seed+config → identical jobs.
///  5.  WorkloadGenerator: different seeds can produce different workloads.
///  6.  WorkloadGenerator: invalid config is rejected safely.
///  7.  WorkloadGenerator: all generated jobs have at least one resource > 0.
///  8.  WorkloadGenerator: job arrivals within configured bound.
///  9.  WorkloadGenerator: job durations within configured bound.
/// 10.  WorkloadGenerator: server counts match config.
/// 11.  WorkloadGenerator: job counts match config.
/// 12.  WorkloadGenerator: servers have positive capacity in at least one dim.
/// 13.  WorkloadGenerator: server IDs are unique.
/// 14.  WorkloadGenerator: job IDs are unique.
/// 15.  StrategyComparison: uses equivalent workloads (fresh state per run).
/// 16.  StrategyComparison: all three strategies produce results.
/// 17.  StrategyComparison: second call to run() produces identical results.
/// 18.  ReportGenerator: N/A shown for undefined optional metrics.
/// 19.  ReportGenerator: all four resource dimensions printed.
/// 20.  ReportGenerator: comparison report contains all strategy names.
/// 21.  End-to-end: workload generated and run through StrategyComparison.
/// 22.  End-to-end: generated workload runs to completion without crashing.
/// 23.  End-to-end: job count from generator matches submitted jobs in metrics.

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

#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

// ─── Micro test harness (identical to earlier milestones) ─────────────────────

struct TestResult { std::string name; bool passed; std::string message; };
static std::vector<TestResult> g_results;
static bool        g_currentPassed  = true;
static std::string g_currentMessage;

static void runTest(const std::string& name, const std::function<void()>& body) {
    g_currentPassed  = true;
    g_currentMessage.clear();
    try { body(); }
    catch (const std::exception& ex) {
        g_currentPassed  = false;
        g_currentMessage += std::string("\n  Unexpected exception: ") + ex.what();
    } catch (...) {
        g_currentPassed  = false;
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

#define EXPECT_NOT_NULL(ptr) \
    do { if ((ptr) == nullptr) { g_currentPassed = false; \
         g_currentMessage += "\n  EXPECT_NOT_NULL: " #ptr " is null"; } } while (false)

#define EXPECT_HAS_VALUE(opt) \
    do { if (!(opt).has_value()) { g_currentPassed = false; \
         g_currentMessage += "\n  EXPECT_HAS_VALUE failed: " #opt " is nullopt"; } } while (false)

#define EXPECT_NO_VALUE(opt) \
    do { if ((opt).has_value()) { g_currentPassed = false; \
         g_currentMessage += "\n  EXPECT_NO_VALUE failed: " #opt " has a value"; } } while (false)

#define EXPECT_GE(a, b) \
    do { if (!((a) >= (b))) { g_currentPassed = false; \
         g_currentMessage += "\n  EXPECT_GE failed: " #a " < " #b; } } while (false)

#define EXPECT_LE(a, b) \
    do { if (!((a) <= (b))) { g_currentPassed = false; \
         g_currentMessage += "\n  EXPECT_LE failed: " #a " > " #b; } } while (false)

#define EXPECT_THROWS(ExType, stmt) \
    do { bool threw_ = false; \
         try { (void)(stmt); } catch (const ExType&) { threw_ = true; } catch (...) {} \
         if (!threw_) { g_currentPassed = false; \
             g_currentMessage += "\n  EXPECT_THROWS(" #ExType ", " #stmt ") did not throw."; } \
    } while (false)

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

// ─── Test suites ──────────────────────────────────────────────────────────────

static void test_WorkloadGeneratorServers() {

    TEST("WG: server count matches config", {
        WorkloadConfig cfg;
        cfg.jobCount    = 10;
        cfg.serverCount = 7;
        cfg.seed        = 1;
        WorkloadGenerator gen{cfg};
        auto servers = gen.generateServers();
        EXPECT_EQ(static_cast<int>(servers.size()), 7);
    });

    TEST("WG: server IDs are unique", {
        WorkloadConfig cfg;
        cfg.serverCount = 10;
        WorkloadGenerator gen{cfg};
        auto servers = gen.generateServers();
        std::set<std::string> ids;
        for (const auto& s : servers) ids.insert(s.id);
        EXPECT_EQ(static_cast<int>(ids.size()), 10);
    });

    TEST("WG: servers have positive capacity in at least one dimension", {
        WorkloadConfig cfg;
        cfg.serverCount = 20;
        WorkloadGenerator gen{cfg};
        auto servers = gen.generateServers();
        for (const auto& s : servers) {
            const auto& c = s.capacity;
            EXPECT_TRUE(c.cpu > 0.0 || c.ram > 0.0 ||
                        c.storage > 0.0 || c.gpu > 0.0);
        }
    });

    TEST("WG: identical seed+config produces identical servers", {
        WorkloadConfig cfg;
        cfg.serverCount = 8;
        cfg.seed        = 99;
        WorkloadGenerator gen1{cfg};
        WorkloadGenerator gen2{cfg};
        auto s1 = gen1.generateServers();
        auto s2 = gen2.generateServers();
        EXPECT_EQ(s1.size(), s2.size());
        for (std::size_t i = 0; i < s1.size(); ++i) {
            EXPECT_EQ(s1[i].id,              s2[i].id);
            EXPECT_EQ(s1[i].capacity.cpu,    s2[i].capacity.cpu);
            EXPECT_EQ(s1[i].capacity.ram,    s2[i].capacity.ram);
            EXPECT_EQ(s1[i].capacity.storage,s2[i].capacity.storage);
            EXPECT_EQ(s1[i].capacity.gpu,    s2[i].capacity.gpu);
        }
    });

    TEST("WG: generateServers() is repeatable on same object", {
        WorkloadConfig cfg;
        cfg.serverCount = 5;
        cfg.seed        = 7;
        WorkloadGenerator gen{cfg};
        auto s1 = gen.generateServers();
        auto s2 = gen.generateServers();   // call again on same generator
        EXPECT_EQ(s1.size(), s2.size());
        for (std::size_t i = 0; i < s1.size(); ++i) {
            EXPECT_EQ(s1[i].id, s2[i].id);
            EXPECT_EQ(s1[i].capacity.cpu, s2[i].capacity.cpu);
        }
    });
}

static void test_WorkloadGeneratorJobs() {

    TEST("WG: job count matches config", {
        WorkloadConfig cfg;
        cfg.jobCount    = 15;
        cfg.serverCount = 3;
        cfg.seed        = 2;
        WorkloadGenerator gen{cfg};
        auto jobs = gen.generateJobs();
        EXPECT_EQ(static_cast<int>(jobs.size()), 15);
    });

    TEST("WG: job IDs are unique", {
        WorkloadConfig cfg;
        cfg.jobCount = 20;
        WorkloadGenerator gen{cfg};
        auto jobs = gen.generateJobs();
        std::set<std::string> ids;
        for (const auto& j : jobs) ids.insert(j.id);
        EXPECT_EQ(static_cast<int>(ids.size()), 20);
    });

    TEST("WG: all jobs have at least one resource > 0", {
        WorkloadConfig cfg;
        cfg.jobCount = 50;
        cfg.seed     = 12345;
        WorkloadGenerator gen{cfg};
        auto jobs = gen.generateJobs();
        for (const auto& j : jobs) {
            const auto& r = j.requirements;
            EXPECT_TRUE(r.cpu > 0.0 || r.ram > 0.0 ||
                        r.storage > 0.0 || r.gpu > 0.0);
        }
    });

    TEST("WG: job arrivals within [0, maxArrival]", {
        WorkloadConfig cfg;
        cfg.jobCount   = 30;
        cfg.maxArrival = 20;
        cfg.seed       = 55;
        WorkloadGenerator gen{cfg};
        auto jobs = gen.generateJobs();
        for (const auto& j : jobs) {
            EXPECT_GE(j.arrivalTick, 0);
            EXPECT_LE(j.arrivalTick, 20);
        }
    });

    TEST("WG: job durations within [1, maxDuration]", {
        WorkloadConfig cfg;
        cfg.jobCount    = 30;
        cfg.maxDuration = 8;
        cfg.seed        = 77;
        WorkloadGenerator gen{cfg};
        auto jobs = gen.generateJobs();
        for (const auto& j : jobs) {
            EXPECT_GE(j.duration, 1);
            EXPECT_LE(j.duration, 8);
        }
    });

    TEST("WG: identical seed+config produces identical jobs", {
        WorkloadConfig cfg;
        cfg.jobCount = 10;
        cfg.seed     = 42;
        WorkloadGenerator gen1{cfg};
        WorkloadGenerator gen2{cfg};
        auto j1 = gen1.generateJobs();
        auto j2 = gen2.generateJobs();
        EXPECT_EQ(j1.size(), j2.size());
        for (std::size_t i = 0; i < j1.size(); ++i) {
            EXPECT_EQ(j1[i].id,                   j2[i].id);
            EXPECT_EQ(j1[i].arrivalTick,           j2[i].arrivalTick);
            EXPECT_EQ(j1[i].duration,              j2[i].duration);
            EXPECT_EQ(j1[i].requirements.cpu,      j2[i].requirements.cpu);
            EXPECT_EQ(j1[i].requirements.ram,      j2[i].requirements.ram);
            EXPECT_EQ(j1[i].requirements.storage,  j2[i].requirements.storage);
            EXPECT_EQ(j1[i].requirements.gpu,      j2[i].requirements.gpu);
        }
    });

    TEST("WG: generateJobs() is repeatable on same object", {
        WorkloadConfig cfg;
        cfg.jobCount = 5;
        cfg.seed     = 3;
        WorkloadGenerator gen{cfg};
        auto j1 = gen.generateJobs();
        auto j2 = gen.generateJobs();
        EXPECT_EQ(j1.size(), j2.size());
        for (std::size_t i = 0; i < j1.size(); ++i) {
            EXPECT_EQ(j1[i].id,          j2[i].id);
            EXPECT_EQ(j1[i].arrivalTick, j2[i].arrivalTick);
            EXPECT_EQ(j1[i].duration,    j2[i].duration);
        }
    });

    TEST("WG: different seeds can produce different jobs", {
        WorkloadConfig cfg;
        cfg.jobCount = 20;
        cfg.seed     = 1;
        WorkloadGenerator gen1{cfg};
        cfg.seed = 999;
        WorkloadGenerator gen2{cfg};

        auto j1 = gen1.generateJobs();
        auto j2 = gen2.generateJobs();

        // Check that at least one job differs in arrival or duration.
        bool anyDiff = false;
        for (std::size_t i = 0; i < j1.size(); ++i) {
            if (j1[i].arrivalTick != j2[i].arrivalTick ||
                j1[i].duration    != j2[i].duration    ||
                j1[i].requirements.cpu != j2[i].requirements.cpu) {
                anyDiff = true;
                break;
            }
        }
        EXPECT_TRUE(anyDiff);
    });

    TEST("WG: maxArrival == 0 gives all arrivals at tick 0", {
        WorkloadConfig cfg;
        cfg.jobCount   = 10;
        cfg.maxArrival = 0;
        cfg.seed       = 5;
        WorkloadGenerator gen{cfg};
        auto jobs = gen.generateJobs();
        for (const auto& j : jobs) {
            EXPECT_EQ(j.arrivalTick, 0);
        }
    });
}

static void test_WorkloadGeneratorInvalidConfig() {

    TEST("WG: jobCount < 1 throws invalid_argument", {
        WorkloadConfig cfg;
        cfg.jobCount = 0;
        EXPECT_THROWS(std::invalid_argument, WorkloadGenerator{cfg});
    });

    TEST("WG: serverCount < 1 throws invalid_argument", {
        WorkloadConfig cfg;
        cfg.serverCount = 0;
        EXPECT_THROWS(std::invalid_argument, WorkloadGenerator{cfg});
    });

    TEST("WG: maxDuration < 1 throws invalid_argument", {
        WorkloadConfig cfg;
        cfg.maxDuration = 0;
        EXPECT_THROWS(std::invalid_argument, WorkloadGenerator{cfg});
    });

    TEST("WG: maxArrival < 0 throws invalid_argument", {
        WorkloadConfig cfg;
        cfg.maxArrival = -1;
        EXPECT_THROWS(std::invalid_argument, WorkloadGenerator{cfg});
    });

    TEST("WG: valid config does not throw", {
        WorkloadConfig cfg;
        cfg.jobCount    = 1;
        cfg.serverCount = 1;
        cfg.maxDuration = 1;
        cfg.maxArrival  = 0;
        bool threw = false;
        try { WorkloadGenerator gen{cfg}; }
        catch (...) { threw = true; }
        EXPECT_FALSE(threw);
    });
}

static void test_StrategyComparisonFreshState() {

    TEST("StrategyComparison: second run() gives identical results (fresh state)", {
        WorkloadConfig cfg;
        cfg.jobCount    = 10;
        cfg.serverCount = 4;
        cfg.seed        = 42;
        WorkloadGenerator gen{cfg};
        auto servers = gen.generateServers();
        auto jobs    = gen.generateJobs();

        StrategyComparison cmp{servers, jobs};
        auto r1 = cmp.run();
        auto r2 = cmp.run();

        EXPECT_EQ(r1.size(), r2.size());
        for (std::size_t i = 0; i < r1.size(); ++i) {
            EXPECT_EQ(r1[i].strategyName, r2[i].strategyName);
            EXPECT_EQ(r1[i].completed,    r2[i].completed);
            EXPECT_EQ(r1[i].rejected,     r2[i].rejected);
            EXPECT_EQ(r1[i].elapsedTicks, r2[i].elapsedTicks);
        }
    });

    TEST("StrategyComparison: all three strategy names present", {
        WorkloadConfig cfg;
        cfg.jobCount    = 5;
        cfg.serverCount = 3;
        cfg.seed        = 1;
        WorkloadGenerator gen{cfg};
        StrategyComparison cmp{gen.generateServers(), gen.generateJobs()};
        auto results = cmp.run();

        EXPECT_EQ(static_cast<int>(results.size()), 3);
        EXPECT_EQ(results[0].strategyName, std::string{"FirstFit"});
        EXPECT_EQ(results[1].strategyName, std::string{"BestFit"});
        EXPECT_EQ(results[2].strategyName, std::string{"LoadBalanced"});
    });

    TEST("StrategyComparison: submitted counts match job spec count", {
        WorkloadConfig cfg;
        cfg.jobCount    = 8;
        cfg.serverCount = 3;
        cfg.seed        = 7;
        WorkloadGenerator gen{cfg};
        auto jobs    = gen.generateJobs();
        auto servers = gen.generateServers();
        StrategyComparison cmp{servers, jobs};
        auto results = cmp.run();

        for (const auto& m : results) {
            EXPECT_EQ(m.totalSubmitted, 8);
        }
    });

    TEST("StrategyComparison: all strategies see the same totalSubmitted", {
        WorkloadConfig cfg;
        cfg.jobCount    = 12;
        cfg.serverCount = 5;
        cfg.seed        = 100;
        WorkloadGenerator gen{cfg};
        StrategyComparison cmp{gen.generateServers(), gen.generateJobs()};
        auto results = cmp.run();

        EXPECT_EQ(results[0].totalSubmitted, results[1].totalSubmitted);
        EXPECT_EQ(results[1].totalSubmitted, results[2].totalSubmitted);
    });
}

static void test_EndToEnd() {

    TEST("E2E: generated workload runs without exception", {
        WorkloadConfig cfg;
        cfg.jobCount    = 20;
        cfg.serverCount = 5;
        cfg.seed        = 42;
        WorkloadGenerator gen{cfg};
        auto servers = gen.generateServers();
        auto jobs    = gen.generateJobs();

        bool threw = false;
        try {
            StrategyComparison cmp{servers, jobs};
            (void)cmp.run();
        } catch (...) {
            threw = true;
        }
        EXPECT_FALSE(threw);
    });

    TEST("E2E: jobCount from generator matches metrics.totalSubmitted", {
        WorkloadConfig cfg;
        cfg.jobCount    = 15;
        cfg.serverCount = 5;
        cfg.seed        = 99;
        WorkloadGenerator gen{cfg};
        auto servers = gen.generateServers();
        auto jobs    = gen.generateJobs();

        StrategyComparison cmp{servers, jobs};
        auto results = cmp.run();
        for (const auto& m : results) {
            EXPECT_EQ(m.totalSubmitted, cfg.jobCount);
        }
    });

    TEST("E2E: completed + rejected + stillActive <= totalSubmitted", {
        WorkloadConfig cfg;
        cfg.jobCount    = 20;
        cfg.serverCount = 4;
        cfg.seed        = 17;
        WorkloadGenerator gen{cfg};
        StrategyComparison cmp{gen.generateServers(), gen.generateJobs()};
        auto results = cmp.run();
        for (const auto& m : results) {
            EXPECT_LE(m.completed + m.rejected + m.stillActive + m.cancelled + m.failed,
                      m.totalSubmitted);
        }
    });

    TEST("E2E: ReportGenerator generates non-empty report", {
        WorkloadConfig cfg;
        cfg.jobCount    = 5;
        cfg.serverCount = 2;
        cfg.seed        = 55;
        WorkloadGenerator gen{cfg};
        StrategyComparison cmp{gen.generateServers(), gen.generateJobs()};
        auto results = cmp.run();
        std::string report = ReportGenerator::generateComparison(results);
        EXPECT_FALSE(report.empty());
        EXPECT_TRUE(report.find("FirstFit")     != std::string::npos);
        EXPECT_TRUE(report.find("BestFit")      != std::string::npos);
        EXPECT_TRUE(report.find("LoadBalanced") != std::string::npos);
    });
}

static void test_ReportingUndefinedMetrics() {

    TEST("Report: N/A for nullopt throughput (zero-job workload)", {
        SimulationMetrics m;
        m.strategyName     = "Test";
        m.elapsedTicks     = 0;
        m.finishedNormally = true;
        // All optionals remain nullopt.
        std::string report = ReportGenerator::generate(m);
        EXPECT_TRUE(report.find("N/A") != std::string::npos);
    });

    TEST("Report: N/A for nullopt utilization", {
        SimulationMetrics m;
        m.strategyName     = "Test";
        m.elapsedTicks     = 1;
        m.finishedNormally = true;
        m.completed        = 1;
        m.totalSubmitted   = 1;
        m.completionRate   = 100.0;
        m.rejectionRate    = 0.0;
        // cpuUtilization etc. remain nullopt.
        std::string report = ReportGenerator::generate(m);
        EXPECT_TRUE(report.find("N/A") != std::string::npos);
    });

    TEST("Report: all four resource dimensions present in output", {
        SimulationMetrics m;
        m.strategyName     = "Test";
        m.finishedNormally = true;
        std::string report = ReportGenerator::generate(m);
        EXPECT_TRUE(report.find("CPU")     != std::string::npos);
        EXPECT_TRUE(report.find("RAM")     != std::string::npos);
        EXPECT_TRUE(report.find("Storage") != std::string::npos);
        EXPECT_TRUE(report.find("GPU")     != std::string::npos);
    });
}

// ─── Audit: utilization display fix ─────────────────────────────────────────
// Utilization values are stored as [0,1] fractions in SimulationMetrics.
// ReportGenerator::generate() must display them multiplied by 100 (as %).
// Before the fix, 0.099 was displayed as "0.1%"; after the fix it should
// display as "9.9%".

static void test_UtilizationDisplayFix() {

    TEST("Audit: 50% CPU utilization displays as '50.0%' not '0.5%'", {
        // Server: 4 cpu.  Job: 2 cpu, duration 1.  elapsed = 1.
        // cpuUtil = (2*1)/(4*1) = 0.5  → should display as '50.0%'.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};
        engine.setWorkload({Job{"j", ResourceRequirements{2.0,0.0,0.0,0.0}, 0, 0, 1}});
        engine.run();
        auto m = MetricsCollector::collect(engine, dc, "FirstFit");

        // Stored value must be the fraction.
        EXPECT_HAS_VALUE(m.cpuUtilization);
        EXPECT_NEAR(m.cpuUtilization.value(), 0.5, 1e-9);

        // Displayed string must be "50.0%" (×100).
        std::string report = ReportGenerator::generate(m);
        EXPECT_TRUE(report.find("50.0%") != std::string::npos);
        // Must NOT contain the raw fraction form.
        EXPECT_FALSE(report.find("0.5%") != std::string::npos);
    });

    TEST("Audit: 100% CPU utilization displays as '100.0%'", {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};
        engine.setWorkload({Job{"j", ResourceRequirements{4.0,0.0,0.0,0.0}, 0, 0, 1}});
        engine.run();
        auto m = MetricsCollector::collect(engine, dc, "FirstFit");
        std::string report = ReportGenerator::generate(m);
        EXPECT_TRUE(report.find("100.0%") != std::string::npos);
    });

    TEST("Audit: completion rate still displays correctly as percentage (no regression)", {
        // completionRate is stored as a percentage (90.0), NOT a fraction.
        // fmtPct should still print '90.0%'.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};
        std::vector<Job> jobs;
        jobs.push_back(Job{"ok",  ResourceRequirements{2.0,0.0,0.0,0.0}, 0, 0, 1});
        jobs.push_back(Job{"big", ResourceRequirements{99.0,0.0,0.0,0.0}, 0, 0, 1});
        engine.setWorkload(std::move(jobs));
        engine.run();
        auto m = MetricsCollector::collect(engine, dc, "FirstFit");
        std::string report = ReportGenerator::generate(m);
        // 50% completion, 50% rejection.
        EXPECT_TRUE(report.find("50.0%") != std::string::npos);
    });

    TEST("Audit: utilization N/A still shown when nullopt (no regression)", {
        SimulationMetrics m;
        m.strategyName     = "Test";
        m.finishedNormally = true;
        // All utilization fields remain nullopt.
        std::string report = ReportGenerator::generate(m);
        EXPECT_TRUE(report.find("N/A") != std::string::npos);
    });
}

// ─── Audit: strategy selection correctness ───────────────────────────────────
// Controlled workload: 2 servers in fixed order, 1 job.
//   large-srv: 16 cpu (listed first)
//   small-srv:  4 cpu (listed second)
//   Job: 2 cpu, arrives tick 0, duration 1.
//
// Expected selections (hand-calculated):
//   FirstFit    → large-srv  (first feasible in order)
//   BestFit     → small-srv  (residual: small=(4-2)/4=0.50 < large=(16-2)/16=0.875)
//   LoadBalanced→ large-srv  (projected util: large=(0+2)/16=0.125 < small=(0+2)/4=0.50)

static void test_StrategySelectionCorrectness() {

    // Helper: run one strategy on the controlled workload and return the
    // serverId logged in the JOB_STARTED event.
    auto serverChosenBy = [](AllocationStrategy& strat) -> std::string {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>(
            "large-srv", ResourceRequirements{16.0, 0.0, 0.0, 0.0}));
        dc.addServer(std::make_unique<ComputeServer>(
            "small-srv", ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        Scheduler sched{strat, dc};
        SimulationEngine engine{sched, dc};
        engine.setWorkload({Job{"j", ResourceRequirements{2.0,0.0,0.0,0.0}, 0, 0, 1}});
        engine.run();
        for (const auto& ev : engine.getEvents()) {
            if (ev.type == EventType::JOB_STARTED) return ev.serverId;
        }
        return "";
    };

    TEST("Audit strategy: FirstFit selects first feasible server (large-srv)", {
        FirstFit ff;
        EXPECT_EQ(serverChosenBy(ff), std::string{"large-srv"});
    });

    TEST("Audit strategy: BestFit selects tighter server (small-srv)", {
        BestFit bf;
        EXPECT_EQ(serverChosenBy(bf), std::string{"small-srv"});
    });

    TEST("Audit strategy: LoadBalanced selects least-loaded server (large-srv)", {
        LoadBalanced lb;
        EXPECT_EQ(serverChosenBy(lb), std::string{"large-srv"});
    });

    TEST("Audit strategy: BestFit and FirstFit pick DIFFERENT servers on this workload", {
        FirstFit ff;
        BestFit  bf;
        std::string ffChoice = serverChosenBy(ff);
        std::string bfChoice = serverChosenBy(bf);
        // FirstFit → large-srv, BestFit → small-srv: they must differ.
        EXPECT_FALSE(ffChoice == bfChoice);
    });

    TEST("Audit strategy: identical workloads produce consistent results across runs", {
        // Run FirstFit twice on the same specs; server chosen must be identical.
        FirstFit ff1, ff2;
        EXPECT_EQ(serverChosenBy(ff1), serverChosenBy(ff2));
    });
}

// ─── Audit: waiting time with genuine queue pressure ─────────────────────────
// Verifies that avgWaitingTime correctly reflects queue delays, not just
// immediate starts.

static void test_WaitingTimeAudit() {

    TEST("Audit wait: seed=42 workload zero wait is valid (no contention)", {
        // Jobs in the seed=42 workload start immediately at their arrival ticks
        // because server capacity is large relative to job requirements.
        // avgWaitingTime == 0.0 is correct, not a bug.
        WorkloadConfig cfg;
        cfg.jobCount    = 20;
        cfg.serverCount = 5;
        cfg.seed        = 42;
        WorkloadGenerator gen{cfg};
        StrategyComparison cmp{gen.generateServers(), gen.generateJobs()};
        auto results = cmp.run();
        for (const auto& m : results) {
            EXPECT_HAS_VALUE(m.avgWaitingTime);
            // All jobs should have started immediately (zero wait) for this
            // low-contention workload.
            EXPECT_NEAR(m.avgWaitingTime.value(), 0.0, 1e-9);
        }
    });

    TEST("Audit wait: tight capacity produces non-zero avgWaitingTime", {
        // Server: 4 cpu.  j1: 4 cpu dur=3, j2: 4 cpu, arr=0.
        // j2 must wait until tick 3 when j1 finishes.
        // waitTime_j1 = 0, waitTime_j2 = 3.  avg = 1.5.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};
        std::vector<Job> jobs;
        jobs.push_back(Job{"j1", ResourceRequirements{4.0,0.0,0.0,0.0}, 0, 0, 3});
        jobs.push_back(Job{"j2", ResourceRequirements{4.0,0.0,0.0,0.0}, 0, 0, 1});
        engine.setWorkload(std::move(jobs));
        engine.run();
        auto m = MetricsCollector::collect(engine, dc, "FirstFit");
        EXPECT_HAS_VALUE(m.avgWaitingTime);
        EXPECT_NEAR(m.avgWaitingTime.value(), 1.5, 1e-9);
        EXPECT_EQ(m.waitingTimeSampleCount, 2);
    });
}

// ─── Audit: throughput and elapsed time ─────────────────────────────────────

static void test_ThroughputAudit() {

    TEST("Audit throughput: seed=42 baseline — 18 jobs / 54 ticks = 0.333", {
        // Verify the baseline throughput is arithmetically correct:
        // 18 completed / 54 elapsed = 0.3333...
        WorkloadConfig cfg;
        cfg.jobCount    = 20;
        cfg.serverCount = 5;
        cfg.seed        = 42;
        WorkloadGenerator gen{cfg};
        auto servers = gen.generateServers();
        auto jobs    = gen.generateJobs();
        StrategyComparison cmp{servers, jobs};
        auto results = cmp.run();
        for (const auto& m : results) {
            EXPECT_EQ(m.completed, 18);
            EXPECT_EQ(m.elapsedTicks, 54);
            EXPECT_HAS_VALUE(m.throughput);
            EXPECT_NEAR(m.throughput.value(), 18.0/54.0, 1e-9);
        }
    });

    TEST("Audit throughput: elapsed = last completion tick (not last arrival)", {
        // Job arrives at tick 5, starts tick 5, duration 3 -> finishes tick 8.
        // Elapsed must be 8 (the last completion), not 5 (the arrival).
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};
        engine.setWorkload({Job{"j", ResourceRequirements{2.0,0.0,0.0,0.0}, 0, 5, 3}});
        engine.run();
        auto m = MetricsCollector::collect(engine, dc, "FirstFit");
        EXPECT_EQ(m.elapsedTicks, 8);
        EXPECT_HAS_VALUE(m.throughput);
        EXPECT_NEAR(m.throughput.value(), 1.0/8.0, 1e-9);
    });
}

// ─── Runner ───────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== Orchestrix Milestone 5 + Audit Tests ===\n\n";

    test_WorkloadGeneratorServers();
    test_WorkloadGeneratorJobs();
    test_WorkloadGeneratorInvalidConfig();
    test_StrategyComparisonFreshState();
    test_EndToEnd();
    test_ReportingUndefinedMetrics();
    test_UtilizationDisplayFix();
    test_StrategySelectionCorrectness();
    test_WaitingTimeAudit();
    test_ThroughputAudit();

    int passed = 0, failed = 0;
    for (const auto& r : g_results) {
        if (r.passed) { std::cout << "  [PASS] " << r.name << "\n"; ++passed; }
        else          { std::cout << "  [FAIL] " << r.name << r.message << "\n"; ++failed; }
    }

    std::cout << "\n--- " << passed << " passed, " << failed << " failed ---\n";
    return (failed > 0) ? 1 : 0;
}
