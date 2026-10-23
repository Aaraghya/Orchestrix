/// test_milestone2.cpp — Automated tests for Milestone 2: Allocation Strategies & Scheduler
///
/// Uses the same micro-harness as test_milestone1.cpp.
/// The harness is duplicated here so that each test binary is self-contained.
///
/// Coverage:
///  - FirstFit: correct server selected, order preserved, no feasible server.
///  - BestFit:  scoring formula, tie-breaking, zero job requirements.
///  - LoadBalanced: scoring formula, tie-breaking, zero-capacity dimensions.
///  - All strategies: resource dimensions not modified by select().
///  - Incompatible server types are skipped by all strategies.
///  - Scheduler: successful allocation (job transitions to RUNNING, resources reserved).
///  - Scheduler: no feasible server (job stays QUEUED, no server state modified).
///  - Scheduler: pre-condition violations throw.
///  - Scheduler: strategy can be swapped at runtime.

#include "AllocationStrategy.h"
#include "DataCenter.h"
#include "Job.h"
#include "ResourceRequirements.h"
#include "Scheduler.h"
#include "Server.h"

#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// ─── Micro test harness (same pattern as test_milestone1.cpp) ─────────────────

struct TestResult {
    std::string name;
    bool        passed;
    std::string message;
};

static std::vector<TestResult> g_results;
static bool        g_currentPassed  = true;
static std::string g_currentMessage;

static void runTest(const std::string& name, const std::function<void()>& body) {
    g_currentPassed  = true;
    g_currentMessage.clear();
    try {
        body();
    } catch (const std::exception& ex) {
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

#define EXPECT_NULL(ptr) \
    do { if ((ptr) != nullptr) { g_currentPassed = false; \
         g_currentMessage += "\n  EXPECT_NULL failed: " #ptr " is not null"; } } while (false)

#define EXPECT_NOT_NULL(ptr) \
    do { if ((ptr) == nullptr) { g_currentPassed = false; \
         g_currentMessage += "\n  EXPECT_NOT_NULL failed: " #ptr " is null"; } } while (false)

#define EXPECT_THROWS(ExType, stmt) \
    do { bool threw_ = false; \
         try { (void)(stmt); } catch (const ExType&) { threw_ = true; } catch (...) {} \
         if (!threw_) { g_currentPassed = false; \
             g_currentMessage += "\n  EXPECT_THROWS(" #ExType ", " #stmt ") did not throw."; } \
    } while (false)

// ─── Helpers ──────────────────────────────────────────────────────────────────

/// Build a standard three-server DataCenter used across many tests.
///
///   "cs-a"   ComputeServer  cpu=8  ram=16  storage=200  gpu=0
///   "cs-b"   ComputeServer  cpu=4  ram=8   storage=100  gpu=0
///   "gpu-a"  GPUServer      cpu=8  ram=16  storage=100  gpu=4
///
/// Insertion order: cs-a, cs-b, gpu-a.
static DataCenter makeStandardDC() {
    DataCenter dc;
    dc.addServer(std::make_unique<ComputeServer>("cs-a",
        ResourceRequirements{8.0, 16.0, 200.0, 0.0}));
    dc.addServer(std::make_unique<ComputeServer>("cs-b",
        ResourceRequirements{4.0, 8.0, 100.0, 0.0}));
    dc.addServer(std::make_unique<GPUServer>("gpu-a",
        ResourceRequirements{8.0, 16.0, 100.0, 4.0}));
    return dc;
}

/// Make a simple queued job (arrives at tick 0).
static Job makeQueuedJob(const std::string& id,
                         const ResourceRequirements& req,
                         int duration = 1) {
    Job j{id, req, 0, 0, duration};
    j.arrive();   // PENDING → QUEUED
    return j;
}

// ─── FirstFit tests ───────────────────────────────────────────────────────────

static void test_FirstFit() {

    TEST("FirstFit: selects first compatible server in order", {
        // Job needs cpu=2, ram=4 — fits on cs-a (first in list).
        DataCenter dc = makeStandardDC();
        ResourceRequirements req{2.0, 4.0, 0.0, 0.0};
        FirstFit ff;
        Server* result = ff.select(dc.getServers(), req);
        EXPECT_NOT_NULL(result);
        EXPECT_EQ(result->getId(), std::string{"cs-a"});
    });

    TEST("FirstFit: skips first server when it cannot accommodate", {
        // Saturate cs-a first, then a new request should land on cs-b.
        DataCenter dc = makeStandardDC();
        // Fill cs-a completely.
        dc.findServer("cs-a")->reserveResources(ResourceRequirements{8.0, 16.0, 200.0, 0.0});

        ResourceRequirements req{2.0, 4.0, 0.0, 0.0};
        FirstFit ff;
        Server* result = ff.select(dc.getServers(), req);
        EXPECT_NOT_NULL(result);
        EXPECT_EQ(result->getId(), std::string{"cs-b"});
    });

    TEST("FirstFit: returns nullptr when no server can accommodate", {
        DataCenter dc = makeStandardDC();
        // Request more CPU than any server has.
        ResourceRequirements req{100.0, 4.0, 0.0, 0.0};
        FirstFit ff;
        Server* result = ff.select(dc.getServers(), req);
        EXPECT_NULL(result);
    });

    TEST("FirstFit: skips incompatible server types", {
        // A GPU job should skip the two ComputeServers in favour of the GPUServer.
        // But FirstFit returns the first COMPATIBLE server, and ComputeServer
        // accepts all jobs.  So a GPU job that also fits on cs-a will land on cs-a.
        // This test verifies that a CPU-only job does NOT land on the GPUServer.
        DataCenter dc;
        dc.addServer(std::make_unique<GPUServer>("gpu-only",
            ResourceRequirements{8.0, 16.0, 100.0, 4.0}));
        dc.addServer(std::make_unique<ComputeServer>("comp",
            ResourceRequirements{4.0, 8.0, 100.0, 0.0}));

        ResourceRequirements cpuJob{2.0, 4.0, 0.0, 0.0}; // gpu == 0
        FirstFit ff;
        Server* result = ff.select(dc.getServers(), cpuJob);
        // GPUServer rejects jobs with gpu == 0, so should land on comp.
        EXPECT_NOT_NULL(result);
        EXPECT_EQ(result->getId(), std::string{"comp"});
    });

    TEST("FirstFit: returns nullptr for empty server list", {
        DataCenter dc;
        ResourceRequirements req{2.0, 4.0, 0.0, 0.0};
        FirstFit ff;
        Server* result = ff.select(dc.getServers(), req);
        EXPECT_NULL(result);
    });

    TEST("FirstFit: select does not modify server state", {
        DataCenter dc = makeStandardDC();
        ResourceRequirements req{2.0, 4.0, 0.0, 0.0};
        auto allocBefore = dc.findServer("cs-a")->getAllocated();
        FirstFit ff;
        (void)ff.select(dc.getServers(), req);
        auto allocAfter = dc.findServer("cs-a")->getAllocated();
        EXPECT_EQ(allocBefore.cpu, allocAfter.cpu);
        EXPECT_EQ(allocBefore.ram, allocAfter.ram);
    });
}

// ─── BestFit tests ────────────────────────────────────────────────────────────

static void test_BestFit() {

    TEST("BestFit: selects tighter fit over looser fit", {
        // Two servers: cs-large (16 cpu, 32 ram) and cs-small (4 cpu, 8 ram).
        // Job needs 2 cpu, 4 ram.
        // cs-large residual: (14/16) + (28/32) = 0.875 + 0.875 = 1.75
        // cs-small residual: (2/4)   + (4/8)   = 0.5   + 0.5   = 1.0
        // BestFit should select cs-small (lower score = tighter fit).
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs-large",
            ResourceRequirements{16.0, 32.0, 0.0, 0.0}));
        dc.addServer(std::make_unique<ComputeServer>("cs-small",
            ResourceRequirements{4.0, 8.0, 0.0, 0.0}));

        ResourceRequirements req{2.0, 4.0, 0.0, 0.0};
        BestFit bf;
        Server* result = bf.select(dc.getServers(), req);
        EXPECT_NOT_NULL(result);
        EXPECT_EQ(result->getId(), std::string{"cs-small"});
    });

    TEST("BestFit: excludes zero-requirement dimensions from scoring", {
        // Job needs only cpu=2 (ram=0, storage=0, gpu=0).
        // Only the cpu dimension is scored.
        // cs-a: 8 cpu total, residual after 2 = (6/8) = 0.75
        // cs-b: 4 cpu total, residual after 2 = (2/4) = 0.50
        // BestFit selects cs-b.
        DataCenter dc = makeStandardDC();
        ResourceRequirements req{2.0, 0.0, 0.0, 0.0};
        BestFit bf;
        Server* result = bf.select(dc.getServers(), req);
        EXPECT_NOT_NULL(result);
        EXPECT_EQ(result->getId(), std::string{"cs-b"});
    });

    TEST("BestFit: tie-breaking uses lexicographically smaller server ID", {
        // Two identical servers; same score → pick the one with smaller ID.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("srv-b",
            ResourceRequirements{4.0, 8.0, 0.0, 0.0}));
        dc.addServer(std::make_unique<ComputeServer>("srv-a",
            ResourceRequirements{4.0, 8.0, 0.0, 0.0}));

        ResourceRequirements req{1.0, 2.0, 0.0, 0.0};
        BestFit bf;
        Server* result = bf.select(dc.getServers(), req);
        EXPECT_NOT_NULL(result);
        // "srv-a" < "srv-b" lexicographically → should be selected.
        EXPECT_EQ(result->getId(), std::string{"srv-a"});
    });

    TEST("BestFit: returns nullptr when no server is feasible", {
        DataCenter dc = makeStandardDC();
        ResourceRequirements req{100.0, 4.0, 0.0, 0.0};
        BestFit bf;
        EXPECT_NULL(bf.select(dc.getServers(), req));
    });

    TEST("BestFit: all-zero job requirements treated correctly", {
        // A job with all-zero requirements fits anywhere.
        // Score for any dimension d with req==0 is excluded → score = 0.0 for each.
        // Both servers get score 0; tie-broken by ID.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("b-srv",
            ResourceRequirements{4.0, 8.0, 0.0, 0.0}));
        dc.addServer(std::make_unique<ComputeServer>("a-srv",
            ResourceRequirements{4.0, 8.0, 0.0, 0.0}));

        ResourceRequirements req{0.0, 0.0, 0.0, 0.0};
        BestFit bf;
        Server* result = bf.select(dc.getServers(), req);
        EXPECT_NOT_NULL(result);
        // "a-srv" < "b-srv" → selected by tie-break.
        EXPECT_EQ(result->getId(), std::string{"a-srv"});
    });

    TEST("BestFit: skips servers that cannot accommodate (type mismatch)", {
        // Only a StorageServer is present; job needs no storage → no candidate.
        DataCenter dc;
        dc.addServer(std::make_unique<StorageServer>("stor",
            ResourceRequirements{4.0, 8.0, 5000.0, 0.0}));

        ResourceRequirements req{2.0, 4.0, 0.0, 0.0}; // storage == 0
        BestFit bf;
        EXPECT_NULL(bf.select(dc.getServers(), req));
    });

    TEST("BestFit: select does not modify server state", {
        DataCenter dc = makeStandardDC();
        ResourceRequirements req{2.0, 4.0, 0.0, 0.0};
        auto beforeA = dc.findServer("cs-a")->getAllocated();
        auto beforeB = dc.findServer("cs-b")->getAllocated();
        BestFit bf;
        (void)bf.select(dc.getServers(), req);
        EXPECT_EQ(dc.findServer("cs-a")->getAllocated().cpu, beforeA.cpu);
        EXPECT_EQ(dc.findServer("cs-b")->getAllocated().cpu, beforeB.cpu);
    });

    TEST("BestFit: prefers server already partially loaded when it fits tighter", {
        // cs-heavy has 4 cpu used out of 8; cs-empty is fresh with 8 cpu.
        // Job needs 2 cpu.
        // cs-heavy available cpu = 4, score contribution = (4-2)/8 = 0.25
        // cs-empty available cpu = 8, score contribution = (8-2)/8 = 0.75
        // BestFit selects cs-heavy (lower score).
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs-heavy",
            ResourceRequirements{8.0, 0.0, 0.0, 0.0}));
        dc.addServer(std::make_unique<ComputeServer>("cs-empty",
            ResourceRequirements{8.0, 0.0, 0.0, 0.0}));
        // Load cs-heavy.
        dc.findServer("cs-heavy")->reserveResources(ResourceRequirements{4.0, 0.0, 0.0, 0.0});

        ResourceRequirements req{2.0, 0.0, 0.0, 0.0};
        BestFit bf;
        Server* result = bf.select(dc.getServers(), req);
        EXPECT_NOT_NULL(result);
        EXPECT_EQ(result->getId(), std::string{"cs-heavy"});
    });
}

// ─── LoadBalanced tests ───────────────────────────────────────────────────────

static void test_LoadBalanced() {

    TEST("LoadBalanced: selects server with lower projected utilisation", {
        // cs-a: 8 cpu, 16 ram (all free). After job (2 cpu, 4 ram):
        //   util_cpu = 2/8 = 0.25, util_ram = 4/16 = 0.25 → avg = 0.25
        // cs-b: 4 cpu, 8 ram (all free). After job (2 cpu, 4 ram):
        //   util_cpu = 2/4 = 0.50, util_ram = 4/8  = 0.50 → avg = 0.50
        // LoadBalanced selects cs-a (lower average).
        DataCenter dc = makeStandardDC();
        ResourceRequirements req{2.0, 4.0, 0.0, 0.0};
        LoadBalanced lb;
        Server* result = lb.select(dc.getServers(), req);
        EXPECT_NOT_NULL(result);
        EXPECT_EQ(result->getId(), std::string{"cs-a"});
    });

    TEST("LoadBalanced: excludes dimensions with zero total capacity", {
        // cs-nogpu has gpu total = 0; only cpu and ram count.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs-nogpu",
            ResourceRequirements{4.0, 8.0, 0.0, 0.0}));
        ResourceRequirements req{2.0, 4.0, 0.0, 0.0};
        LoadBalanced lb;
        Server* result = lb.select(dc.getServers(), req);
        EXPECT_NOT_NULL(result);
        EXPECT_EQ(result->getId(), std::string{"cs-nogpu"});
    });

    TEST("LoadBalanced: tie-breaking uses lexicographically smaller server ID", {
        // Two identical servers; same score → pick the one with smaller ID.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("z-srv",
            ResourceRequirements{4.0, 8.0, 0.0, 0.0}));
        dc.addServer(std::make_unique<ComputeServer>("a-srv",
            ResourceRequirements{4.0, 8.0, 0.0, 0.0}));

        ResourceRequirements req{1.0, 2.0, 0.0, 0.0};
        LoadBalanced lb;
        Server* result = lb.select(dc.getServers(), req);
        EXPECT_NOT_NULL(result);
        EXPECT_EQ(result->getId(), std::string{"a-srv"});
    });

    TEST("LoadBalanced: returns nullptr when no server is feasible", {
        DataCenter dc = makeStandardDC();
        ResourceRequirements req{100.0, 4.0, 0.0, 0.0};
        LoadBalanced lb;
        EXPECT_NULL(lb.select(dc.getServers(), req));
    });

    TEST("LoadBalanced: prefers emptier server after partial loading", {
        // cs-loaded: 8 cpu, 4 already used. After job(2): util = 6/8 = 0.75
        // cs-empty:  8 cpu, 0 used.          After job(2): util = 2/8 = 0.25
        // LoadBalanced picks cs-empty.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs-loaded",
            ResourceRequirements{8.0, 0.0, 0.0, 0.0}));
        dc.addServer(std::make_unique<ComputeServer>("cs-empty",
            ResourceRequirements{8.0, 0.0, 0.0, 0.0}));
        dc.findServer("cs-loaded")->reserveResources(ResourceRequirements{4.0, 0.0, 0.0, 0.0});

        ResourceRequirements req{2.0, 0.0, 0.0, 0.0};
        LoadBalanced lb;
        Server* result = lb.select(dc.getServers(), req);
        EXPECT_NOT_NULL(result);
        EXPECT_EQ(result->getId(), std::string{"cs-empty"});
    });

    TEST("LoadBalanced: skips incompatible server types", {
        DataCenter dc;
        dc.addServer(std::make_unique<StorageServer>("stor",
            ResourceRequirements{4.0, 8.0, 5000.0, 0.0}));
        ResourceRequirements req{2.0, 4.0, 0.0, 0.0};
        LoadBalanced lb;
        EXPECT_NULL(lb.select(dc.getServers(), req));
    });

    TEST("LoadBalanced: select does not modify server state", {
        DataCenter dc = makeStandardDC();
        ResourceRequirements req{2.0, 4.0, 0.0, 0.0};
        auto beforeA = dc.findServer("cs-a")->getAllocated();
        LoadBalanced lb;
        (void)lb.select(dc.getServers(), req);
        EXPECT_EQ(dc.findServer("cs-a")->getAllocated().cpu, beforeA.cpu);
    });

    TEST("LoadBalanced: all-zero job requirements handled", {
        // Zero-resource job: util_d = (allocated_d + 0) / total_d = allocated_d / total_d.
        // Fresh servers all have utilisation 0; tie broken by ID.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("beta",
            ResourceRequirements{4.0, 8.0, 0.0, 0.0}));
        dc.addServer(std::make_unique<ComputeServer>("alpha",
            ResourceRequirements{4.0, 8.0, 0.0, 0.0}));
        ResourceRequirements req{0.0, 0.0, 0.0, 0.0};
        LoadBalanced lb;
        Server* result = lb.select(dc.getServers(), req);
        EXPECT_NOT_NULL(result);
        EXPECT_EQ(result->getId(), std::string{"alpha"});
    });
}

// ─── Cross-strategy consistency tests ────────────────────────────────────────

static void test_StrategyConsistency() {

    TEST("All strategies: return nullptr for empty server list", {
        DataCenter dc;
        ResourceRequirements req{2.0, 4.0, 0.0, 0.0};
        FirstFit    ff;
        BestFit     bf;
        LoadBalanced lb;
        EXPECT_NULL(ff.select(dc.getServers(), req));
        EXPECT_NULL(bf.select(dc.getServers(), req));
        EXPECT_NULL(lb.select(dc.getServers(), req));
    });

    TEST("All strategies: return a feasible server when one exists", {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("only",
            ResourceRequirements{8.0, 16.0, 0.0, 0.0}));
        ResourceRequirements req{2.0, 4.0, 0.0, 0.0};
        FirstFit    ff;
        BestFit     bf;
        LoadBalanced lb;
        EXPECT_NOT_NULL(ff.select(dc.getServers(), req));
        EXPECT_NOT_NULL(bf.select(dc.getServers(), req));
        EXPECT_NOT_NULL(lb.select(dc.getServers(), req));
    });

    TEST("All strategies: name() returns non-empty string", {
        FirstFit    ff;
        BestFit     bf;
        LoadBalanced lb;
        EXPECT_FALSE(ff.name().empty());
        EXPECT_FALSE(bf.name().empty());
        EXPECT_FALSE(lb.name().empty());
    });

    TEST("Repeated select() calls do not change server state", {
        DataCenter dc = makeStandardDC();
        ResourceRequirements req{2.0, 4.0, 0.0, 0.0};
        FirstFit    ff;
        BestFit     bf;
        LoadBalanced lb;

        auto snap = [&]() { return dc.findServer("cs-a")->getAllocated().cpu; };
        double before = snap();

        for (int i = 0; i < 5; ++i) {
            (void)ff.select(dc.getServers(), req);
            (void)bf.select(dc.getServers(), req);
            (void)lb.select(dc.getServers(), req);
        }

        EXPECT_EQ(snap(), before);
    });
}

// ─── Scheduler tests ──────────────────────────────────────────────────────────

static void test_Scheduler() {

    TEST("Scheduler: successful allocation transitions job to RUNNING", {
        DataCenter dc = makeStandardDC();
        FirstFit ff;
        Scheduler sched{ff, dc};

        ResourceRequirements req{2.0, 4.0, 0.0, 0.0};
        Job job = makeQueuedJob("j1", req);

        Server* placed = sched.scheduleJob(job, 0);
        EXPECT_NOT_NULL(placed);
        EXPECT_EQ(job.getState(), JobState::RUNNING);
        EXPECT_TRUE(job.getAssignedServerId().has_value());
        EXPECT_EQ(job.getStartTick().value(), 0);
    });

    TEST("Scheduler: successful allocation reserves resources on chosen server", {
        DataCenter dc = makeStandardDC();
        FirstFit ff;
        Scheduler sched{ff, dc};

        ResourceRequirements req{2.0, 4.0, 0.0, 0.0};
        Job job = makeQueuedJob("j2", req);

        Server* placed = sched.scheduleJob(job, 0);
        EXPECT_NOT_NULL(placed);
        EXPECT_EQ(placed->getAllocated().cpu, 2.0);
        EXPECT_EQ(placed->getAllocated().ram, 4.0);
    });

    TEST("Scheduler: no feasible server leaves job QUEUED", {
        DataCenter dc = makeStandardDC();
        FirstFit ff;
        Scheduler sched{ff, dc};

        // Request more resources than any server has.
        ResourceRequirements req{100.0, 4.0, 0.0, 0.0};
        Job job = makeQueuedJob("j3", req);

        Server* placed = sched.scheduleJob(job, 0);
        EXPECT_NULL(placed);
        EXPECT_EQ(job.getState(), JobState::QUEUED);
    });

    TEST("Scheduler: no feasible server does not modify any server state", {
        DataCenter dc = makeStandardDC();
        FirstFit ff;
        Scheduler sched{ff, dc};

        ResourceRequirements req{100.0, 4.0, 0.0, 0.0};
        Job job = makeQueuedJob("j-nofeasible", req);

        auto cpuBefore = dc.findServer("cs-a")->getAllocated().cpu;
        (void)sched.scheduleJob(job, 0);
        EXPECT_EQ(dc.findServer("cs-a")->getAllocated().cpu, cpuBefore);
    });

    TEST("Scheduler: throws when job is not QUEUED", {
        DataCenter dc = makeStandardDC();
        FirstFit ff;
        Scheduler sched{ff, dc};

        ResourceRequirements req{2.0, 4.0, 0.0, 0.0};
        Job job{"j-pending", req, 0, 0, 1}; // still PENDING

        EXPECT_THROWS(std::logic_error, sched.scheduleJob(job, 0));
    });

    TEST("Scheduler: throws when currentTick is before arrivalTick", {
        DataCenter dc = makeStandardDC();
        FirstFit ff;
        Scheduler sched{ff, dc};

        ResourceRequirements req{2.0, 4.0, 0.0, 0.0};
        Job job{"j-future", req, 0, 5, 1};
        job.arrive(); // QUEUED, arrivalTick == 5

        EXPECT_THROWS(std::logic_error, sched.scheduleJob(job, 3));
    });

    TEST("Scheduler: strategy can be swapped and new strategy is used", {
        DataCenter dc = makeStandardDC();
        FirstFit    ff;
        LoadBalanced lb;
        Scheduler sched{ff, dc};

        // With FirstFit, a cpu=2 job lands on cs-a (first server).
        ResourceRequirements req{2.0, 4.0, 0.0, 0.0};
        Job j1 = makeQueuedJob("jff", req);
        Server* placed1 = sched.scheduleJob(j1, 0);
        EXPECT_NOT_NULL(placed1);
        EXPECT_EQ(placed1->getId(), std::string{"cs-a"});

        // Release resources so the test is clean for the next round.
        placed1->releaseResources(req);
        // Re-queue j1 is not possible, so use a new job.
        Job j2 = makeQueuedJob("jlb", req);

        sched.setStrategy(lb);
        EXPECT_EQ(sched.strategyName(), std::string{"LoadBalanced"});
        Server* placed2 = sched.scheduleJob(j2, 0);
        EXPECT_NOT_NULL(placed2);
        // LoadBalanced with two fresh servers picks cs-a (larger → lower utilisation after placement).
        EXPECT_EQ(placed2->getId(), std::string{"cs-a"});
    });

    TEST("Scheduler: BestFit places job on tighter server", {
        // cs-a (8 cpu) and cs-b (4 cpu); job needs 2 cpu.
        // BestFit should pick cs-b (tighter).
        DataCenter dc = makeStandardDC();
        BestFit bf;
        Scheduler sched{bf, dc};

        ResourceRequirements req{2.0, 0.0, 0.0, 0.0};
        Job job = makeQueuedJob("jbf", req);
        Server* placed = sched.scheduleJob(job, 0);
        EXPECT_NOT_NULL(placed);
        EXPECT_EQ(placed->getId(), std::string{"cs-b"});
    });

    TEST("Scheduler: strategyName matches the configured strategy", {
        DataCenter dc = makeStandardDC();
        FirstFit ff;
        Scheduler sched{ff, dc};
        EXPECT_EQ(sched.strategyName(), std::string{"FirstFit"});
    });

    TEST("Scheduler: job with GPU requirement is directed to GPUServer", {
        // ComputeServer in position [0] is compatible (accepts all), but
        // the GPU job can also go there.  FirstFit picks the first compatible.
        // We test with a DataCenter where only a GPUServer exists.
        DataCenter dc;
        dc.addServer(std::make_unique<GPUServer>("gpu-only",
            ResourceRequirements{8.0, 16.0, 100.0, 4.0}));

        ResourceRequirements req{2.0, 4.0, 10.0, 1.0};
        FirstFit ff;
        Scheduler sched{ff, dc};

        Job job = makeQueuedJob("jgpu", req);
        Server* placed = sched.scheduleJob(job, 0);
        EXPECT_NOT_NULL(placed);
        EXPECT_EQ(placed->getId(), std::string{"gpu-only"});
        EXPECT_EQ(placed->getAllocated().gpu, 1.0);
    });

    TEST("Scheduler: multiple consecutive allocations are independent", {
        DataCenter dc = makeStandardDC();
        FirstFit ff;
        Scheduler sched{ff, dc};

        ResourceRequirements req{2.0, 4.0, 0.0, 0.0};

        Job j1 = makeQueuedJob("c1", req);
        Job j2 = makeQueuedJob("c2", req);
        Job j3 = makeQueuedJob("c3", req);

        // cs-a has 8 cpu; three jobs of 2 cpu each fit.
        Server* p1 = sched.scheduleJob(j1, 0);
        Server* p2 = sched.scheduleJob(j2, 0);
        Server* p3 = sched.scheduleJob(j3, 0);

        EXPECT_NOT_NULL(p1);
        EXPECT_NOT_NULL(p2);
        EXPECT_NOT_NULL(p3);

        // After three 2-cpu allocations on cs-a (cap 8), allocated should be 6.
        EXPECT_EQ(dc.findServer("cs-a")->getAllocated().cpu, 6.0);
    });
}

// ─── Runner ───────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== Orchestrix Milestone 2 — Automated Tests ===\n\n";

    test_FirstFit();
    test_BestFit();
    test_LoadBalanced();
    test_StrategyConsistency();
    test_Scheduler();

    int passed = 0;
    int failed = 0;
    for (const auto& r : g_results) {
        if (r.passed) {
            std::cout << "  [PASS] " << r.name << "\n";
            ++passed;
        } else {
            std::cout << "  [FAIL] " << r.name << r.message << "\n";
            ++failed;
        }
    }

    std::cout << "\n--- " << passed << " passed, " << failed << " failed ---\n";
    return (failed > 0) ? 1 : 0;
}
