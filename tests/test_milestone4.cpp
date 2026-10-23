/// test_milestone4.cpp — Automated tests for Milestone 4: Metrics & Reporting
///
/// Hand-calculated expected values are used wherever possible.
/// Uses the same micro-harness as previous milestones.
///
/// Coverage:
///  1.  Correct job outcome counts.
///  2.  Completion and rejection percentages (including zero-job workload).
///  3.  Waiting time for multiple jobs with different arrival/start ticks.
///  4.  No-started-jobs waiting-time behavior (nullopt).
///  5.  Throughput and zero-elapsed-time behavior.
///  6.  Resource utilization over time (hand-calculated).
///  7.  Independent utilization per dimension.
///  8.  Zero-capacity resource dimensions.
///  9.  Same-tick arrivals and completions.
/// 10.  Simulation stopped by max-tick guard.
/// 11.  Strategy comparisons use identical workload and fresh state.
/// 12.  Comparison results are deterministic.
/// 13.  Reports include all required fields; undefined metrics show N/A.
/// 14.  All Milestones 1–3 tests continue to pass (built and run separately).

#include "AllocationStrategy.h"
#include "DataCenter.h"
#include "Job.h"
#include "MetricsCollector.h"
#include "ReportGenerator.h"
#include "ResourceRequirements.h"
#include "Scheduler.h"
#include "SimulationEngine.h"
#include "SimulationMetrics.h"
#include "StrategyComparison.h"
#include "Server.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// ─── Micro test harness ───────────────────────────────────────────────────────

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

#define EXPECT_THROWS(ExType, stmt) \
    do { bool threw_ = false; \
         try { (void)(stmt); } catch (const ExType&) { threw_ = true; } catch (...) {} \
         if (!threw_) { g_currentPassed = false; \
             g_currentMessage += "\n  EXPECT_THROWS(" #ExType ", " #stmt ") did not throw."; } \
    } while (false)

// ─── Helpers ──────────────────────────────────────────────────────────────────

static const double EPS = 1e-9;

/// Run a simulation and collect metrics in one call.
/// Returns the metrics.
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

/// Build a PENDING job.
static Job makeJob(const std::string& id,
                   const ResourceRequirements& req,
                   int arrivalTick = 0,
                   int duration    = 1,
                   int priority    = 0)
{
    return Job{id, req, priority, arrivalTick, duration};
}

// ─── Test suites ──────────────────────────────────────────────────────────────

static void test_JobOutcomeCounts() {

    TEST("Metrics: all jobs complete — counts correct", {
        // 3 jobs, each completes. Submitted=3, Completed=3.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{12.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        for (int i = 0; i < 3; ++i)
            jobs.push_back(makeJob("j" + std::to_string(i),
                ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 1));

        auto m = runAndCollect(dc, sched, std::move(jobs));
        EXPECT_EQ(m.totalSubmitted, 3);
        EXPECT_EQ(m.completed,      3);
        EXPECT_EQ(m.rejected,       0);
        EXPECT_EQ(m.stillActive,    0);
    });

    TEST("Metrics: impossible job is rejected — counts correct", {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("ok",  ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 1));
        jobs.push_back(makeJob("big", ResourceRequirements{99.0, 0.0, 0.0, 0.0}, 0, 1));

        auto m = runAndCollect(dc, sched, std::move(jobs));
        EXPECT_EQ(m.totalSubmitted, 2);
        EXPECT_EQ(m.completed,      1);
        EXPECT_EQ(m.rejected,       1);
    });

    TEST("Metrics: zero-job workload — all counts zero, rates nullopt", {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        auto m = runAndCollect(dc, sched, {});
        EXPECT_EQ(m.totalSubmitted, 0);
        EXPECT_EQ(m.completed,      0);
        EXPECT_EQ(m.rejected,       0);
        EXPECT_NO_VALUE(m.completionRate);
        EXPECT_NO_VALUE(m.rejectionRate);
    });

    TEST("Metrics: completion and rejection rates correct", {
        // 2 jobs: 1 completes (50%), 1 rejected (50%)
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{2.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("ok",  ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 1));
        jobs.push_back(makeJob("big", ResourceRequirements{99.0, 0.0, 0.0, 0.0}, 0, 1));

        auto m = runAndCollect(dc, sched, std::move(jobs));
        EXPECT_HAS_VALUE(m.completionRate);
        EXPECT_HAS_VALUE(m.rejectionRate);
        EXPECT_NEAR(m.completionRate.value(), 50.0, EPS);
        EXPECT_NEAR(m.rejectionRate.value(),  50.0, EPS);
    });

    TEST("Metrics: 100% completion rate", {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j1", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 1));
        jobs.push_back(makeJob("j2", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 1));

        auto m = runAndCollect(dc, sched, std::move(jobs));
        EXPECT_HAS_VALUE(m.completionRate);
        EXPECT_NEAR(m.completionRate.value(), 100.0, EPS);
        EXPECT_HAS_VALUE(m.rejectionRate);
        EXPECT_NEAR(m.rejectionRate.value(),    0.0, EPS);
    });

    TEST("Metrics: max-tick guard — stillActive counted correctly", {
        // Job arrives at tick 100, engine limited to tick 2.
        // Job stays PENDING → counts as stillActive.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("late", ResourceRequirements{1.0, 0.0, 0.0, 0.0}, 100, 1));
        auto m = runAndCollect(dc, sched, std::move(jobs), /*maxTick=*/2);

        EXPECT_TRUE(m.hitMaxTick);
        EXPECT_EQ(m.stillActive, 1);
        EXPECT_EQ(m.completed,   0);
    });
}

static void test_WaitingTime() {

    TEST("Metrics: single job, waiting time = startTick - arrivalTick", {
        // Job arrives at tick 0, no wait, starts immediately → waitTime = 0.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j1", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 1));
        auto m = runAndCollect(dc, sched, std::move(jobs));
        EXPECT_HAS_VALUE(m.avgWaitingTime);
        EXPECT_NEAR(m.avgWaitingTime.value(), 0.0, EPS);
        EXPECT_EQ(m.waitingTimeSampleCount, 1);
    });

    TEST("Metrics: job waits because server full — waiting time > 0", {
        // j1 arrives tick 0, uses all 4 cpu for 3 ticks (finishes tick 3).
        // j2 arrives tick 0, same req → must wait until tick 3.
        // j2 waitTime = 3 - 0 = 3.
        // avg = (0 + 3) / 2 = 1.5.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j1", ResourceRequirements{4.0, 0.0, 0.0, 0.0}, 0, 3));
        jobs.push_back(makeJob("j2", ResourceRequirements{4.0, 0.0, 0.0, 0.0}, 0, 1));
        auto m = runAndCollect(dc, sched, std::move(jobs));
        EXPECT_HAS_VALUE(m.avgWaitingTime);
        EXPECT_NEAR(m.avgWaitingTime.value(), 1.5, EPS);
        EXPECT_EQ(m.waitingTimeSampleCount, 2);
    });

    TEST("Metrics: job arrives late — waiting time correct", {
        // j1 arrives tick 5, starts at tick 5 → waitTime = 0.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j1", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 5, 1));
        auto m = runAndCollect(dc, sched, std::move(jobs));
        EXPECT_HAS_VALUE(m.avgWaitingTime);
        EXPECT_NEAR(m.avgWaitingTime.value(), 0.0, EPS);
    });

    TEST("Metrics: no job started — waiting time is nullopt", {
        // Only one rejected job → nothing started.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{1.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("big", ResourceRequirements{100.0, 0.0, 0.0, 0.0}, 0, 1));
        auto m = runAndCollect(dc, sched, std::move(jobs));
        EXPECT_NO_VALUE(m.avgWaitingTime);
        EXPECT_EQ(m.waitingTimeSampleCount, 0);
    });

    TEST("Metrics: multiple jobs different wait times — average correct", {
        // Server: 2 cpu.
        // j1: arrives 0, starts 0 → wait 0.
        // j2: arrives 0, starts 1 → wait 1.  (j1 uses 2 cpu for 1 tick)
        // avg = (0 + 1) / 2 = 0.5.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{2.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j1", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 1));
        jobs.push_back(makeJob("j2", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 1));
        auto m = runAndCollect(dc, sched, std::move(jobs));
        EXPECT_HAS_VALUE(m.avgWaitingTime);
        EXPECT_NEAR(m.avgWaitingTime.value(), 0.5, EPS);
    });
}

static void test_Throughput() {

    TEST("Metrics: throughput = completed / elapsedTicks", {
        // 2 jobs each duration 1, both start at tick 0, finish tick 1.
        // completed=2, elapsed=1 → throughput=2.0.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j1", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 1));
        jobs.push_back(makeJob("j2", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 1));
        auto m = runAndCollect(dc, sched, std::move(jobs));
        EXPECT_HAS_VALUE(m.throughput);
        EXPECT_NEAR(m.throughput.value(), 2.0, EPS);
    });

    TEST("Metrics: zero elapsed ticks — throughput is nullopt", {
        // Empty workload: engine stops at tick 0, elapsed=0.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};
        auto m = runAndCollect(dc, sched, {});
        EXPECT_NO_VALUE(m.throughput);
    });

    TEST("Metrics: throughput with longer duration", {
        // 1 job, duration 4 → starts tick 0, finishes tick 4.
        // completed=1, elapsed=4 → throughput = 0.25.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{2.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j1", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 4));
        auto m = runAndCollect(dc, sched, std::move(jobs));
        EXPECT_HAS_VALUE(m.throughput);
        EXPECT_NEAR(m.throughput.value(), 0.25, EPS);
    });
}

static void test_ResourceUtilization() {

    TEST("Metrics: 100% CPU utilization — one job filling server for entire run", {
        // Server: 4 cpu.  Job: 4 cpu for 1 tick.
        // allocCpuTime = 4 * 1 = 4.  totalCpuTime = 4 * 1 = 4.
        // utilization = 4/4 = 1.0.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j", ResourceRequirements{4.0, 0.0, 0.0, 0.0}, 0, 1));
        auto m = runAndCollect(dc, sched, std::move(jobs));
        EXPECT_HAS_VALUE(m.cpuUtilization);
        EXPECT_NEAR(m.cpuUtilization.value(), 1.0, EPS);
    });

    TEST("Metrics: 50% CPU utilization — half cores used for whole run", {
        // Server: 8 cpu.  Job: 4 cpu for 1 tick.
        // allocCpuTime = 4 * 1 = 4.  totalCpuTime = 8 * 1 = 8.
        // utilization = 4/8 = 0.5.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{8.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j", ResourceRequirements{4.0, 0.0, 0.0, 0.0}, 0, 1));
        auto m = runAndCollect(dc, sched, std::move(jobs));
        EXPECT_HAS_VALUE(m.cpuUtilization);
        EXPECT_NEAR(m.cpuUtilization.value(), 0.5, EPS);
    });

    TEST("Metrics: resource-time over multiple ticks (the 4-cores/5-ticks example)", {
        // Server: 8 cpu.  Job: 4 cpu, duration 5.
        // allocCpuTime = 4 * 5 = 20 core-ticks.
        // Run ends at tick 5 (finishTick).  elapsed = 5.
        // totalCpuTime = 8 * 5 = 40.
        // utilization = 20/40 = 0.5.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{8.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j", ResourceRequirements{4.0, 0.0, 0.0, 0.0}, 0, 5));
        auto m = runAndCollect(dc, sched, std::move(jobs));
        EXPECT_HAS_VALUE(m.cpuUtilization);
        EXPECT_NEAR(m.cpuUtilization.value(), 0.5, EPS);  // 20/40
    });

    TEST("Metrics: job starts mid-simulation — partial utilization accounted", {
        // Server: 4 cpu.
        // j1: starts tick 0, duration 2, finishes tick 2. (uses 2 ticks of 4 cpu)
        // j2: arrives tick 2, starts tick 2, duration 1, finishes tick 3.
        // elapsed = 3.
        // allocCpuTime = 4*2 + 4*1 = 12.
        // totalCpuTime = 4*3 = 12.
        // utilization = 12/12 = 1.0.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j1", ResourceRequirements{4.0, 0.0, 0.0, 0.0}, 0, 2));
        jobs.push_back(makeJob("j2", ResourceRequirements{4.0, 0.0, 0.0, 0.0}, 2, 1));
        auto m = runAndCollect(dc, sched, std::move(jobs));
        EXPECT_HAS_VALUE(m.cpuUtilization);
        EXPECT_NEAR(m.cpuUtilization.value(), 1.0, EPS);
    });

    TEST("Metrics: utilization < 1 when job delayed mid-simulation", {
        // Server: 4 cpu.
        // j1: starts tick 0, duration 1, finishes tick 1.
        // j2: arrives tick 3, starts tick 3, duration 1, finishes tick 4.
        // elapsed = 4.
        // allocCpuTime = 4*1 + 4*1 = 8.
        // totalCpuTime = 4*4 = 16.
        // utilization = 8/16 = 0.5.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j1", ResourceRequirements{4.0, 0.0, 0.0, 0.0}, 0, 1));
        jobs.push_back(makeJob("j2", ResourceRequirements{4.0, 0.0, 0.0, 0.0}, 3, 1));
        auto m = runAndCollect(dc, sched, std::move(jobs));
        EXPECT_HAS_VALUE(m.cpuUtilization);
        EXPECT_NEAR(m.cpuUtilization.value(), 0.5, EPS);
    });

    TEST("Metrics: zero-capacity dimension gives nullopt utilization", {
        // ComputeServer with no GPU capacity.  GPU utilization must be nullopt.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 8.0, 0.0, 0.0}));  // gpu cap = 0
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j", ResourceRequirements{2.0, 4.0, 0.0, 0.0}, 0, 1));
        auto m = runAndCollect(dc, sched, std::move(jobs));
        EXPECT_HAS_VALUE(m.cpuUtilization);
        EXPECT_HAS_VALUE(m.ramUtilization);
        EXPECT_NO_VALUE(m.gpuUtilization);
        EXPECT_NO_VALUE(m.storageUtilization);
    });

    TEST("Metrics: independent CPU and RAM utilization", {
        // Server: 4 cpu, 8 ram.  Job: 2 cpu, 4 ram, duration 1.
        // cpuUtil = (2*1)/(4*1) = 0.5
        // ramUtil = (4*1)/(8*1) = 0.5
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 8.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j", ResourceRequirements{2.0, 4.0, 0.0, 0.0}, 0, 1));
        auto m = runAndCollect(dc, sched, std::move(jobs));
        EXPECT_HAS_VALUE(m.cpuUtilization);
        EXPECT_HAS_VALUE(m.ramUtilization);
        EXPECT_NEAR(m.cpuUtilization.value(), 0.5, EPS);
        EXPECT_NEAR(m.ramUtilization.value(), 0.5, EPS);
    });

    TEST("Metrics: GPU utilization tracked independently", {
        // GPUServer: 8 cpu, 16 ram, 4 gpu.  Job: 4 cpu, 8 ram, 2 gpu, duration 2.
        // elapsed = 2.
        // cpuUtil  = (4*2)/(8*2)  = 8/16  = 0.5
        // ramUtil  = (8*2)/(16*2) = 16/32 = 0.5
        // gpuUtil  = (2*2)/(4*2)  = 4/8   = 0.5
        DataCenter dc;
        dc.addServer(std::make_unique<GPUServer>("gpu",
            ResourceRequirements{8.0, 16.0, 0.0, 4.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j", ResourceRequirements{4.0, 8.0, 0.0, 2.0}, 0, 2));
        auto m = runAndCollect(dc, sched, std::move(jobs));
        EXPECT_HAS_VALUE(m.cpuUtilization);
        EXPECT_HAS_VALUE(m.ramUtilization);
        EXPECT_HAS_VALUE(m.gpuUtilization);
        EXPECT_NEAR(m.cpuUtilization.value(), 0.5, EPS);
        EXPECT_NEAR(m.ramUtilization.value(), 0.5, EPS);
        EXPECT_NEAR(m.gpuUtilization.value(), 0.5, EPS);
    });

    TEST("Metrics: same-tick completion and arrival — utilization correct", {
        // j1: arrives 0, starts 0, duration 2, finishes 2.
        // j2: arrives 2, starts 2, duration 1, finishes 3.
        // Server: 4 cpu. elapsed=3.
        // allocCpu = 4*2 + 4*1 = 12. totalCpu = 4*3 = 12. util = 1.0
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j1", ResourceRequirements{4.0, 0.0, 0.0, 0.0}, 0, 2));
        jobs.push_back(makeJob("j2", ResourceRequirements{4.0, 0.0, 0.0, 0.0}, 2, 1));
        auto m = runAndCollect(dc, sched, std::move(jobs));
        EXPECT_HAS_VALUE(m.cpuUtilization);
        EXPECT_NEAR(m.cpuUtilization.value(), 1.0, EPS);
    });

    TEST("Metrics: zero elapsed ticks — all utilizations nullopt", {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 8.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};
        auto m = runAndCollect(dc, sched, {});
        EXPECT_NO_VALUE(m.cpuUtilization);
        EXPECT_NO_VALUE(m.ramUtilization);
    });

    TEST("Metrics: utilization aggregated across two servers", {
        // Two servers each with 4 cpu.  Total capacity = 8 cpu per tick.
        // Job A: 4 cpu, duration 2 → 8 cpu-ticks.
        // Job B: 4 cpu, duration 2 → 8 cpu-ticks (runs simultaneously on server 2).
        // elapsed = 2.  totalCpu = (4+4)*2 = 16.  allocCpu = 8+8 = 16.
        // utilization = 16/16 = 1.0.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs1",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        dc.addServer(std::make_unique<ComputeServer>("cs2",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("jA", ResourceRequirements{4.0, 0.0, 0.0, 0.0}, 0, 2));
        jobs.push_back(makeJob("jB", ResourceRequirements{4.0, 0.0, 0.0, 0.0}, 0, 2));
        auto m = runAndCollect(dc, sched, std::move(jobs));
        EXPECT_HAS_VALUE(m.cpuUtilization);
        EXPECT_NEAR(m.cpuUtilization.value(), 1.0, EPS);
    });
}

static void test_StrategyComparisonSuite() {

    TEST("StrategyComparison: returns three results in FirstFit/BestFit/LoadBalanced order", {
        StrategyComparison cmp{
            { {ServerKind::Compute, "cs", ResourceRequirements{4.0, 0.0, 0.0, 0.0}} },
            { {"j1", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 0, 1} }
        };
        auto results = cmp.run();
        EXPECT_EQ(static_cast<int>(results.size()), 3);
        EXPECT_EQ(results[0].strategyName, std::string{"FirstFit"});
        EXPECT_EQ(results[1].strategyName, std::string{"BestFit"});
        EXPECT_EQ(results[2].strategyName, std::string{"LoadBalanced"});
    });

    TEST("StrategyComparison: all strategies complete the workload", {
        StrategyComparison cmp{
            { {ServerKind::Compute, "cs", ResourceRequirements{8.0, 0.0, 0.0, 0.0}} },
            { {"j1", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 0, 1},
              {"j2", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 0, 1} }
        };
        auto results = cmp.run();
        for (const auto& m : results) {
            EXPECT_EQ(m.completed,      2);
            EXPECT_EQ(m.rejected,       0);
            EXPECT_EQ(m.totalSubmitted, 2);
        }
    });

    TEST("StrategyComparison: fresh state — runs are independent", {
        // We verify that metrics from run 1 don't pollute run 2.
        // Two separate cmp.run() calls should give identical results.
        StrategyComparison cmp{
            { {ServerKind::Compute, "cs", ResourceRequirements{4.0, 0.0, 0.0, 0.0}} },
            { {"j1", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 0, 1} }
        };
        auto r1 = cmp.run();
        auto r2 = cmp.run();

        EXPECT_EQ(r1.size(), r2.size());
        for (std::size_t i = 0; i < r1.size(); ++i) {
            EXPECT_EQ(r1[i].strategyName,    r2[i].strategyName);
            EXPECT_EQ(r1[i].completed,       r2[i].completed);
            EXPECT_EQ(r1[i].elapsedTicks,    r2[i].elapsedTicks);
        }
    });

    TEST("StrategyComparison: results are deterministic", {
        // Run the comparison twice with the same specs; results must match.
        std::vector<ServerSpec> servers = {
            {ServerKind::Compute, "big",   ResourceRequirements{8.0, 0.0, 0.0, 0.0}},
            {ServerKind::Compute, "small", ResourceRequirements{4.0, 0.0, 0.0, 0.0}}
        };
        std::vector<JobSpec> jobs = {
            {"jA", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 0, 2},
            {"jB", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 0, 1}
        };

        StrategyComparison cmp1{servers, jobs};
        StrategyComparison cmp2{servers, jobs};
        auto r1 = cmp1.run();
        auto r2 = cmp2.run();

        for (std::size_t i = 0; i < r1.size(); ++i) {
            EXPECT_EQ(r1[i].completed,    r2[i].completed);
            EXPECT_EQ(r1[i].elapsedTicks, r2[i].elapsedTicks);
        }
    });
}

static void test_Reporting() {

    TEST("Report: contains strategy name", {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};
        auto m = runAndCollect(dc, sched, {});
        m.strategyName = "FirstFit";

        std::string report = ReportGenerator::generate(m);
        EXPECT_TRUE(report.find("FirstFit") != std::string::npos);
    });

    TEST("Report: N/A appears for nullopt metrics (empty workload)", {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};
        auto m = runAndCollect(dc, sched, {});
        std::string report = ReportGenerator::generate(m);
        EXPECT_TRUE(report.find("N/A") != std::string::npos);
    });

    TEST("Report: contains 'Completed normally' when no max-tick", {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 1));
        auto m = runAndCollect(dc, sched, std::move(jobs));
        std::string report = ReportGenerator::generate(m);
        EXPECT_TRUE(report.find("Completed normally") != std::string::npos);
    });

    TEST("Report: comparison report contains all three strategy sections", {
        StrategyComparison cmp{
            { {ServerKind::Compute, "cs", ResourceRequirements{4.0, 0.0, 0.0, 0.0}} },
            { {"j1", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 0, 1} }
        };
        auto results = cmp.run();
        std::string report = ReportGenerator::generateComparison(results);

        EXPECT_TRUE(report.find("FirstFit")    != std::string::npos);
        EXPECT_TRUE(report.find("BestFit")     != std::string::npos);
        EXPECT_TRUE(report.find("LoadBalanced") != std::string::npos);
    });

    TEST("Report: contains CPU utilization field", {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 8.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j", ResourceRequirements{2.0, 4.0, 0.0, 0.0}, 0, 1));
        auto m = runAndCollect(dc, sched, std::move(jobs));
        std::string report = ReportGenerator::generate(m);
        EXPECT_TRUE(report.find("CPU") != std::string::npos);
        EXPECT_TRUE(report.find("RAM") != std::string::npos);
    });

    TEST("Report: max-tick message present when guard fires", {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("late", ResourceRequirements{1.0, 0.0, 0.0, 0.0}, 100, 1));
        auto m = runAndCollect(dc, sched, std::move(jobs), /*maxTick=*/2);
        std::string report = ReportGenerator::generate(m);
        EXPECT_TRUE(report.find("max-tick") != std::string::npos ||
                    report.find("STOPPED")  != std::string::npos);
    });

    TEST("Metrics: collect throws when engine has not run", {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};
        engine.setWorkload({});
        EXPECT_THROWS(std::logic_error,
            MetricsCollector::collect(engine, dc, "FirstFit"));
    });
}

// ─── Runner ───────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== Orchestrix Milestone 4 — Automated Tests ===\n\n";

    test_JobOutcomeCounts();
    test_WaitingTime();
    test_Throughput();
    test_ResourceUtilization();
    test_StrategyComparisonSuite();
    test_Reporting();

    int passed = 0, failed = 0;
    for (const auto& r : g_results) {
        if (r.passed) { std::cout << "  [PASS] " << r.name << "\n"; ++passed; }
        else          { std::cout << "  [FAIL] " << r.name << r.message << "\n"; ++failed; }
    }

    std::cout << "\n--- " << passed << " passed, " << failed << " failed ---\n";
    return (failed > 0) ? 1 : 0;
}
