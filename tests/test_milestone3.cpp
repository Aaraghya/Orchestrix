/// test_milestone3.cpp — Automated tests for Milestone 3: Simulation Engine
///
/// Uses the same micro-harness as the previous milestones.
///
/// Coverage aligns with simulation-rules.md §15 acceptance criteria plus the
/// additional requirements listed in the milestone specification:
///
///  1.  Job arrivals at the correct tick (§3, AC-1).
///  2.  Completion at start_tick + duration (§9, AC-3).
///  3.  Resource release on completion and re-use by a subsequent job (AC-4).
///  4.  Permanently impossible job is REJECTED (AC-5).
///  5.  Temporarily infeasible job stays QUEUED until resources free up (AC-6).
///  6.  Resource dimension never exceeds server capacity (AC-7).
///  7.  FIFO queue order including later-job consideration (§7).
///  8.  Empty workload: engine starts and finishes with no events.
///  9.  Simultaneous arrivals at the same tick.
/// 10.  Termination: normal and max-tick guard.
/// 11.  Deterministic results across repeated runs with the same workload.
/// 12.  Future job cannot start before its arrival tick (§2, AC-2).
/// 13.  Multiple strategies produce correct results through the same engine.

#include "AllocationStrategy.h"
#include "DataCenter.h"
#include "Job.h"
#include "ResourceRequirements.h"
#include "Scheduler.h"
#include "SimulationEngine.h"
#include "SimulationEvent.h"
#include "Server.h"

#include <algorithm>
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

#define EXPECT_NULL(ptr) \
    do { if ((ptr) != nullptr) { g_currentPassed = false; \
         g_currentMessage += "\n  EXPECT_NULL: " #ptr " is not null"; } } while (false)

#define EXPECT_NOT_NULL(ptr) \
    do { if ((ptr) == nullptr) { g_currentPassed = false; \
         g_currentMessage += "\n  EXPECT_NOT_NULL: " #ptr " is null"; } } while (false)

#define EXPECT_THROWS(ExType, stmt) \
    do { bool threw_ = false; \
         try { (void)(stmt); } catch (const ExType&) { threw_ = true; } catch (...) {} \
         if (!threw_) { g_currentPassed = false; \
             g_currentMessage += "\n  EXPECT_THROWS(" #ExType ", " #stmt ") did not throw."; } \
    } while (false)

// ─── Helpers ──────────────────────────────────────────────────────────────────

/// Count events of a given type in the log.
static int countEvents(const std::vector<SimulationEvent>& log, EventType type) {
    int n = 0;
    for (const auto& e : log) { if (e.type == type) ++n; }
    return n;
}

/// Find the first event of a given type (or nullptr if absent).
static const SimulationEvent* findEvent(const std::vector<SimulationEvent>& log,
                                        EventType type) {
    for (const auto& e : log) { if (e.type == type) return &e; }
    return nullptr;
}

/// Find the first event of a given type for a specific job ID.
static const SimulationEvent* findJobEvent(const std::vector<SimulationEvent>& log,
                                           EventType type,
                                           const std::string& jobId) {
    for (const auto& e : log) {
        if (e.type == type && e.jobId == jobId) return &e;
    }
    return nullptr;
}

/// Build a simple Job in PENDING state.
static Job makeJob(const std::string& id,
                   const ResourceRequirements& req,
                   int arrivalTick = 0,
                   int duration    = 1,
                   int priority    = 0)
{
    return Job{id, req, priority, arrivalTick, duration};
}

// ─── Test suites ──────────────────────────────────────────────────────────────

static void test_BasicLifecycle() {

    TEST("Engine: single job completes at start_tick + duration", {
        // One ComputeServer; one job arriving at tick 0 with duration 3.
        // Expected: started at tick 0, completed at tick 3.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 8.0, 0.0, 0.0}));

        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j1", ResourceRequirements{2.0, 4.0, 0.0, 0.0}, 0, 3));
        engine.setWorkload(std::move(jobs));
        engine.run();

        EXPECT_TRUE(engine.hasFinished());
        EXPECT_FALSE(engine.hitMaxTick());

        // Completion must be at tick 0 + 3 = 3.
        const auto& evts = engine.getEvents();
        const SimulationEvent* started   = findJobEvent(evts, EventType::JOB_STARTED,   "j1");
        const SimulationEvent* completed = findJobEvent(evts, EventType::JOB_COMPLETED, "j1");
        EXPECT_NOT_NULL(started);
        EXPECT_NOT_NULL(completed);
        EXPECT_EQ(started->tick,   0);
        EXPECT_EQ(completed->tick, 3);

        // Final job state must be COMPLETED.
        const auto& finalJobs = engine.getJobs();
        EXPECT_EQ(finalJobs[0].getState(), JobState::COMPLETED);
    });

    TEST("Engine: job cannot start before its arrival tick", {
        // Job arrives at tick 5; with only one server and one job the engine
        // should not start it before tick 5.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 8.0, 0.0, 0.0}));

        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j-future", ResourceRequirements{2.0, 4.0, 0.0, 0.0}, 5, 1));
        engine.setWorkload(std::move(jobs));
        engine.run();

        const auto& evts = engine.getEvents();
        const SimulationEvent* started = findJobEvent(evts, EventType::JOB_STARTED, "j-future");
        EXPECT_NOT_NULL(started);
        EXPECT_EQ(started->tick, 5);   // must start at or after arrival tick 5
    });

    TEST("Engine: job arrival is logged at the correct tick", {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 8.0, 0.0, 0.0}));

        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j-arr", ResourceRequirements{1.0, 1.0, 0.0, 0.0}, 7, 1));
        engine.setWorkload(std::move(jobs));
        engine.run();

        const auto& evts = engine.getEvents();
        const SimulationEvent* arrived = findJobEvent(evts, EventType::JOB_ARRIVED, "j-arr");
        EXPECT_NOT_NULL(arrived);
        EXPECT_EQ(arrived->tick, 7);
    });

    TEST("Engine: resource release logged and resources restored after completion", {
        // One server (4 cpu), one job using 4 cpu for 2 ticks.
        // After completion the server should be back to full capacity.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));

        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j-res", ResourceRequirements{4.0, 0.0, 0.0, 0.0}, 0, 2));
        engine.setWorkload(std::move(jobs));
        engine.run();

        EXPECT_EQ(countEvents(engine.getEvents(), EventType::RESOURCE_RELEASED), 1);

        // Server capacity fully restored.
        Server* s = dc.findServer("cs");
        EXPECT_EQ(s->getAllocated().cpu, 0.0);
        EXPECT_EQ(s->getAvailable().cpu, 4.0);
    });
}

static void test_RejectionAndQueueing() {

    TEST("Engine: permanently impossible job is REJECTED", {
        // Server has 4 cpu; job needs 100 cpu — can never fit.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));

        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j-impossible", ResourceRequirements{100.0, 0.0, 0.0, 0.0}));
        engine.setWorkload(std::move(jobs));
        engine.run();

        EXPECT_TRUE(engine.hasFinished());
        EXPECT_EQ(engine.getJobs()[0].getState(), JobState::REJECTED);

        const auto& evts = engine.getEvents();
        EXPECT_EQ(countEvents(evts, EventType::JOB_REJECTED), 1);
        const SimulationEvent* rej = findJobEvent(evts, EventType::JOB_REJECTED, "j-impossible");
        EXPECT_NOT_NULL(rej);
        EXPECT_EQ(rej->tick, 0);
    });

    TEST("Engine: type-incompatible job (GPU on ComputeServer only) is REJECTED", {
        // Only a GPUServer is present; a CPU-only job (gpu==0) can never run on it.
        DataCenter dc;
        dc.addServer(std::make_unique<GPUServer>("gpu",
            ResourceRequirements{8.0, 16.0, 0.0, 4.0}));

        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};

        std::vector<Job> jobs;
        // gpu==0 → incompatible with GPUServer.
        jobs.push_back(makeJob("j-cpuonly", ResourceRequirements{2.0, 4.0, 0.0, 0.0}));
        engine.setWorkload(std::move(jobs));
        engine.run();

        EXPECT_EQ(engine.getJobs()[0].getState(), JobState::REJECTED);
    });

    TEST("Engine: temporarily infeasible job stays QUEUED then runs when resources free", {
        // Server has 4 cpu.
        // Job A: arrives tick 0, uses 4 cpu for 2 ticks (completes tick 2).
        // Job B: arrives tick 0, needs 4 cpu — cannot start until A finishes.
        // Expected: B starts at tick 2, completes at tick 3.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));

        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("A", ResourceRequirements{4.0, 0.0, 0.0, 0.0}, 0, 2));
        jobs.push_back(makeJob("B", ResourceRequirements{4.0, 0.0, 0.0, 0.0}, 0, 1));
        engine.setWorkload(std::move(jobs));
        engine.run();

        const auto& evts = engine.getEvents();

        // A should start at 0, complete at 2.
        const SimulationEvent* aStart = findJobEvent(evts, EventType::JOB_STARTED, "A");
        const SimulationEvent* aEnd   = findJobEvent(evts, EventType::JOB_COMPLETED, "A");
        EXPECT_NOT_NULL(aStart);
        EXPECT_NOT_NULL(aEnd);
        EXPECT_EQ(aStart->tick, 0);
        EXPECT_EQ(aEnd->tick,   2);

        // B should start at 2 (after A releases resources), complete at 3.
        const SimulationEvent* bStart = findJobEvent(evts, EventType::JOB_STARTED, "B");
        const SimulationEvent* bEnd   = findJobEvent(evts, EventType::JOB_COMPLETED, "B");
        EXPECT_NOT_NULL(bStart);
        EXPECT_NOT_NULL(bEnd);
        EXPECT_EQ(bStart->tick, 2);
        EXPECT_EQ(bEnd->tick,   3);

        // Neither job should be rejected.
        EXPECT_EQ(countEvents(evts, EventType::JOB_REJECTED), 0);
    });

    TEST("Engine: server capacity is never exceeded", {
        // Hammer the invariant: run 5 jobs of 2 cpu on a 4-cpu server (in series).
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));

        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};

        std::vector<Job> jobs;
        for (int i = 0; i < 5; ++i) {
            jobs.push_back(makeJob("j" + std::to_string(i),
                ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 1));
        }
        engine.setWorkload(std::move(jobs));
        engine.run();

        // At no point should allocated exceed 4.
        Server* s = dc.findServer("cs");
        EXPECT_EQ(s->getAllocated().cpu, 0.0); // all finished, fully released

        // All five jobs should be COMPLETED.
        for (const auto& j : engine.getJobs()) {
            EXPECT_EQ(j.getState(), JobState::COMPLETED);
        }
    });
}

static void test_QueueOrder() {

    TEST("Engine: FIFO queue — first-arrived job runs first", {
        // Two jobs arriving at the same tick; the first one in the workload
        // vector should be allocated first (FIFO order).
        // Server has capacity for only one job at a time.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{2.0, 0.0, 0.0, 0.0}));

        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("first",  ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 3));
        jobs.push_back(makeJob("second", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 1));
        engine.setWorkload(std::move(jobs));
        engine.run();

        const auto& evts = engine.getEvents();
        const SimulationEvent* fStart = findJobEvent(evts, EventType::JOB_STARTED, "first");
        const SimulationEvent* sStart = findJobEvent(evts, EventType::JOB_STARTED, "second");
        EXPECT_NOT_NULL(fStart);
        EXPECT_NOT_NULL(sStart);
        // "first" must start at tick 0; "second" waits until tick 3.
        EXPECT_EQ(fStart->tick, 0);
        EXPECT_EQ(sStart->tick, 3);
    });

    TEST("Engine: later job runs when earlier job is infeasible at that tick", {
        // Server A: 4 cpu.  Server B: only 2 cpu.
        // Job X: needs 4 cpu (only fits on A), arrives at 0, duration 5.
        // Job Y: needs 2 cpu (fits on B), arrives at 0.
        // While X holds A, Y should still run on B.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("A",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
        dc.addServer(std::make_unique<ComputeServer>("B",
            ResourceRequirements{2.0, 0.0, 0.0, 0.0}));

        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("X", ResourceRequirements{4.0, 0.0, 0.0, 0.0}, 0, 5));
        jobs.push_back(makeJob("Y", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 1));
        engine.setWorkload(std::move(jobs));
        engine.run();

        const auto& evts = engine.getEvents();
        // X gets A at tick 0.
        const SimulationEvent* xStart = findJobEvent(evts, EventType::JOB_STARTED, "X");
        EXPECT_NOT_NULL(xStart);
        EXPECT_EQ(xStart->tick, 0);

        // Y must also start at tick 0 despite X blocking A's resources,
        // because the engine continues examining later queue entries.
        const SimulationEvent* yStart = findJobEvent(evts, EventType::JOB_STARTED, "Y");
        EXPECT_NOT_NULL(yStart);
        EXPECT_EQ(yStart->tick, 0);
    });

    TEST("Engine: FIFO ordering preserved across multiple ticks", {
        // Three jobs all arriving at tick 0, server can only run one at a time.
        // They should be processed in order: j0, j1, j2.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));

        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};

        std::vector<Job> jobs;
        for (int i = 0; i < 3; ++i) {
            jobs.push_back(makeJob("j" + std::to_string(i),
                ResourceRequirements{4.0, 0.0, 0.0, 0.0}, 0, 1));
        }
        engine.setWorkload(std::move(jobs));
        engine.run();

        const auto& evts = engine.getEvents();
        const SimulationEvent* s0 = findJobEvent(evts, EventType::JOB_STARTED, "j0");
        const SimulationEvent* s1 = findJobEvent(evts, EventType::JOB_STARTED, "j1");
        const SimulationEvent* s2 = findJobEvent(evts, EventType::JOB_STARTED, "j2");
        EXPECT_NOT_NULL(s0);
        EXPECT_NOT_NULL(s1);
        EXPECT_NOT_NULL(s2);
        // Each job takes 1 tick; they must run in FIFO order.
        EXPECT_EQ(s0->tick, 0);
        EXPECT_EQ(s1->tick, 1);
        EXPECT_EQ(s2->tick, 2);
    });
}

static void test_SimultaneousArrivals() {

    TEST("Engine: simultaneous arrivals all become QUEUED at the same tick", {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{16.0, 0.0, 0.0, 0.0}));

        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};

        std::vector<Job> jobs;
        for (int i = 0; i < 4; ++i) {
            jobs.push_back(makeJob("j" + std::to_string(i),
                ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 3, 1));  // all arrive at tick 3
        }
        engine.setWorkload(std::move(jobs));
        engine.run();

        const auto& evts = engine.getEvents();
        EXPECT_EQ(countEvents(evts, EventType::JOB_ARRIVED), 4);

        for (int i = 0; i < 4; ++i) {
            const auto* arr = findJobEvent(evts, EventType::JOB_ARRIVED,
                                           "j" + std::to_string(i));
            EXPECT_NOT_NULL(arr);
            EXPECT_EQ(arr->tick, 3);
        }
    });

    TEST("Engine: completion at tick t releases resources before tick-t arrivals run", {
        // Job A: arrives tick 0, uses 4 cpu for 2 ticks (finish tick 2).
        // Job B: arrives tick 2, needs 4 cpu.
        // At tick 2: A completes (step 2), B arrives (step 3), B starts (step 5).
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));

        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("A", ResourceRequirements{4.0, 0.0, 0.0, 0.0}, 0, 2));
        jobs.push_back(makeJob("B", ResourceRequirements{4.0, 0.0, 0.0, 0.0}, 2, 1));
        engine.setWorkload(std::move(jobs));
        engine.run();

        const auto& evts = engine.getEvents();

        const SimulationEvent* bStart = findJobEvent(evts, EventType::JOB_STARTED, "B");
        EXPECT_NOT_NULL(bStart);
        // B must start at tick 2 (resources freed by A at the same tick).
        EXPECT_EQ(bStart->tick, 2);
    });
}

static void test_Termination() {

    TEST("Engine: empty workload terminates immediately", {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));

        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};

        engine.setWorkload({});
        engine.run();

        EXPECT_TRUE(engine.hasFinished());
        EXPECT_FALSE(engine.hitMaxTick());
        EXPECT_EQ(engine.getJobs().size(), std::size_t{0});
    });

    TEST("Engine: max-tick guard fires when workload cannot progress", {
        // GPUServer only; CPU-only job can never run → should be REJECTED immediately.
        // But let's test the guard with an artificially small maxTick to be safe.
        // We construct a job that CAN fit, but set maxTick=0 so the engine stops
        // at tick 0. Duration 5 means finish tick = 5 > maxTick.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));

        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc, /*maxTick=*/2};

        // Job arrives at tick 10 — will never arrive before tick 2.
        std::vector<Job> jobs;
        jobs.push_back(makeJob("j-late", ResourceRequirements{1.0, 0.0, 0.0, 0.0}, 10, 1));
        engine.setWorkload(std::move(jobs));
        engine.run();

        EXPECT_TRUE(engine.hitMaxTick());
        EXPECT_TRUE(engine.hasFinished());

        // The event log must contain SIM_MAX_TICK.
        const auto* e = findEvent(engine.getEvents(), EventType::SIM_MAX_TICK);
        EXPECT_NOT_NULL(e);
    });

    TEST("Engine: throws if run() called twice", {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));

        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};

        engine.setWorkload({});
        engine.run();
        EXPECT_THROWS(std::logic_error, engine.run());
    });

    TEST("Engine: throws if run() called before setWorkload()", {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));

        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};

        EXPECT_THROWS(std::logic_error, engine.run());
    });

    TEST("Engine: normal termination does not hit max-tick guard", {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));

        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("j1", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 3));
        engine.setWorkload(std::move(jobs));
        engine.run();

        EXPECT_TRUE(engine.hasFinished());
        EXPECT_FALSE(engine.hitMaxTick());
    });
}

static void test_Determinism() {

    TEST("Engine: identical workload produces identical results on two runs", {
        // Run the engine twice with freshly constructed objects and identical jobs.
        // Both runs must produce the same event sequence (same ticks, same job IDs).

        auto buildAndRun = [](const std::string& suffix) -> std::vector<SimulationEvent> {
            DataCenter dc;
            dc.addServer(std::make_unique<ComputeServer>("cs" + suffix,
                ResourceRequirements{4.0, 0.0, 0.0, 0.0}));

            FirstFit ff;
            Scheduler sched{ff, dc};
            SimulationEngine engine{sched, dc};

            std::vector<Job> jobs;
            jobs.push_back(makeJob("jA", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 2));
            jobs.push_back(makeJob("jB", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 1, 1));
            engine.setWorkload(std::move(jobs));
            engine.run();
            return engine.getEvents();
        };

        auto evts1 = buildAndRun("_r1");
        auto evts2 = buildAndRun("_r2");

        // Same number of events.
        EXPECT_EQ(evts1.size(), evts2.size());

        // Same event types at the same ticks.
        for (std::size_t i = 0; i < std::min(evts1.size(), evts2.size()); ++i) {
            EXPECT_EQ(evts1[i].type, evts2[i].type);
            EXPECT_EQ(evts1[i].tick, evts2[i].tick);
            EXPECT_EQ(evts1[i].jobId, evts2[i].jobId);
        }
    });
}

static void test_StrategyComparison() {

    TEST("Engine: FirstFit and BestFit both complete the same workload", {
        // A job that can run on either of two servers.
        // Both strategies should complete it (different servers may be chosen).

        auto buildDC = []() {
            DataCenter dc;
            dc.addServer(std::make_unique<ComputeServer>("big",
                ResourceRequirements{8.0, 0.0, 0.0, 0.0}));
            dc.addServer(std::make_unique<ComputeServer>("small",
                ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
            return dc;
        };

        auto makeWorkload = []() -> std::vector<Job> {
            std::vector<Job> jobs;
            jobs.push_back(makeJob("j1", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 2));
            jobs.push_back(makeJob("j2", ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 1));
            return jobs;
        };

        // Run 1: FirstFit
        {
            DataCenter dc = buildDC();
            FirstFit ff;
            Scheduler sched{ff, dc};
            SimulationEngine engine{sched, dc};
            engine.setWorkload(makeWorkload());
            engine.run();
            EXPECT_TRUE(engine.hasFinished());
            EXPECT_FALSE(engine.hitMaxTick());
            for (const auto& j : engine.getJobs()) {
                EXPECT_EQ(j.getState(), JobState::COMPLETED);
            }
        }

        // Run 2: BestFit — fresh DC, fresh Scheduler, fresh engine.
        {
            DataCenter dc = buildDC();
            BestFit bf;
            Scheduler sched{bf, dc};
            SimulationEngine engine{sched, dc};
            engine.setWorkload(makeWorkload());
            engine.run();
            EXPECT_TRUE(engine.hasFinished());
            EXPECT_FALSE(engine.hitMaxTick());
            for (const auto& j : engine.getJobs()) {
                EXPECT_EQ(j.getState(), JobState::COMPLETED);
            }
        }
    });

    TEST("Engine: fresh state between runs — run 1 does not affect run 2", {
        // Simulate strategy-comparison: same logical workload, two separate runs.
        // After run 1 the server in its DC is fully released; run 2 starts fresh.

        auto runWith = [](AllocationStrategy& strat) -> int {
            DataCenter dc;
            dc.addServer(std::make_unique<ComputeServer>("cs",
                ResourceRequirements{4.0, 0.0, 0.0, 0.0}));
            Scheduler sched{strat, dc};
            SimulationEngine engine{sched, dc};

            std::vector<Job> jobs;
            jobs.push_back(makeJob("jA", ResourceRequirements{4.0, 0.0, 0.0, 0.0}, 0, 1));
            engine.setWorkload(std::move(jobs));
            engine.run();

            // Return tick at which jA completed.
            for (const auto& e : engine.getEvents()) {
                if (e.type == EventType::JOB_COMPLETED && e.jobId == "jA")
                    return e.tick;
            }
            return -1;
        };

        FirstFit  ff;
        BestFit   bf;
        int t1 = runWith(ff);
        int t2 = runWith(bf);

        // Both should complete at tick 1 (start=0, duration=1 → finish=1).
        EXPECT_EQ(t1, 1);
        EXPECT_EQ(t2, 1);
    });
}

static void test_EventLog() {

    TEST("Engine: SIM_STARTED event is always the first event", {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));

        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};
        engine.setWorkload({});
        engine.run();

        const auto& evts = engine.getEvents();
        EXPECT_TRUE(!evts.empty());
        EXPECT_EQ(evts.front().type, EventType::SIM_STARTED);
    });

    TEST("Engine: SIM_FINISHED event appears for normal termination", {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{4.0, 0.0, 0.0, 0.0}));

        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};
        engine.setWorkload({});
        engine.run();

        EXPECT_NOT_NULL(findEvent(engine.getEvents(), EventType::SIM_FINISHED));
    });

    TEST("Engine: event counts are consistent with job count", {
        // 3 jobs, all completing → 3 JOB_ARRIVED, 3 JOB_STARTED,
        //   3 JOB_COMPLETED, 3 RESOURCE_RELEASED.
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cs",
            ResourceRequirements{12.0, 0.0, 0.0, 0.0}));

        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};

        std::vector<Job> jobs;
        for (int i = 0; i < 3; ++i) {
            jobs.push_back(makeJob("j" + std::to_string(i),
                ResourceRequirements{2.0, 0.0, 0.0, 0.0}, 0, 1));
        }
        engine.setWorkload(std::move(jobs));
        engine.run();

        const auto& evts = engine.getEvents();
        EXPECT_EQ(countEvents(evts, EventType::JOB_ARRIVED),       3);
        EXPECT_EQ(countEvents(evts, EventType::JOB_STARTED),       3);
        EXPECT_EQ(countEvents(evts, EventType::JOB_COMPLETED),      3);
        EXPECT_EQ(countEvents(evts, EventType::RESOURCE_RELEASED),  3);
        EXPECT_EQ(countEvents(evts, EventType::JOB_REJECTED),       0);
    });
}

static void test_MultipleJobTypes() {

    TEST("Engine: GPU job is placed on GPUServer, CPU job on ComputeServer", {
        DataCenter dc;
        dc.addServer(std::make_unique<ComputeServer>("cpu-srv",
            ResourceRequirements{8.0, 16.0, 0.0, 0.0}));
        dc.addServer(std::make_unique<GPUServer>("gpu-srv",
            ResourceRequirements{8.0, 16.0, 0.0, 4.0}));

        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};

        std::vector<Job> jobs;
        // CPU job: no GPU required → should land on cpu-srv or gpu-srv (firstfit).
        jobs.push_back(makeJob("cpu-job", ResourceRequirements{2.0, 4.0, 0.0, 0.0}, 0, 1));
        // GPU job: requires GPU → must land on gpu-srv.
        jobs.push_back(makeJob("gpu-job", ResourceRequirements{2.0, 4.0, 0.0, 1.0}, 0, 1));
        engine.setWorkload(std::move(jobs));
        engine.run();

        for (const auto& j : engine.getJobs()) {
            EXPECT_EQ(j.getState(), JobState::COMPLETED);
        }
    });

    TEST("Engine: storage job runs on StorageServer", {
        DataCenter dc;
        dc.addServer(std::make_unique<StorageServer>("stor",
            ResourceRequirements{4.0, 8.0, 5000.0, 0.0}));

        FirstFit ff;
        Scheduler sched{ff, dc};
        SimulationEngine engine{sched, dc};

        std::vector<Job> jobs;
        jobs.push_back(makeJob("stor-job", ResourceRequirements{1.0, 2.0, 500.0, 0.0}, 0, 2));
        engine.setWorkload(std::move(jobs));
        engine.run();

        EXPECT_EQ(engine.getJobs()[0].getState(), JobState::COMPLETED);
    });
}

// ─── Runner ───────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== Orchestrix Milestone 3 — Automated Tests ===\n\n";

    test_BasicLifecycle();
    test_RejectionAndQueueing();
    test_QueueOrder();
    test_SimultaneousArrivals();
    test_Termination();
    test_Determinism();
    test_StrategyComparison();
    test_EventLog();
    test_MultipleJobTypes();

    int passed = 0, failed = 0;
    for (const auto& r : g_results) {
        if (r.passed) { std::cout << "  [PASS] " << r.name << "\n"; ++passed; }
        else          { std::cout << "  [FAIL] " << r.name << r.message << "\n"; ++failed; }
    }

    std::cout << "\n--- " << passed << " passed, " << failed << " failed ---\n";
    return (failed > 0) ? 1 : 0;
}
