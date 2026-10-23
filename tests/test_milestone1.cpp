/// test_milestone1.cpp — Automated tests for Milestone 1: Domain Model
///
/// Test framework: a minimal single-header harness built on the C++ standard
/// library.  No external dependencies are introduced.
///
/// Design note on macro commas
/// ---------------------------
/// The C preprocessor splits macro arguments at every top-level comma, so a
/// call like TEST("name", { foo(a, b); }) would be parsed as more than two
/// arguments.  To avoid this we give TEST a variadic second argument (__VA_ARGS__)
/// and wrap the body in an immediately-invoked lambda so the call site always
/// looks like TEST("name", <single-lambda-expression>).

#include "DataCenter.h"
#include "Job.h"
#include "ResourceRequirements.h"
#include "Server.h"

#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// ─── Micro test harness ───────────────────────────────────────────────────────

struct TestResult {
    std::string name;
    bool        passed;
    std::string message;
};

static std::vector<TestResult> g_results;

// Internal state written to by the macros below.
static bool        g_currentPassed  = true;
static std::string g_currentMessage;

/// Register and immediately run a test body.
/// The body is a void() callable (lambda or function).
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

// TEST wraps the body in a lambda so commas inside it are not macro arguments.
// __VA_ARGS__ is used only to absorb accidental extra commas; the intent is
// always a single braced statement sequence.
#define TEST(name, ...) runTest((name), [&]() { __VA_ARGS__ })

#define EXPECT_TRUE(expr)                                                  \
    do {                                                                   \
        if (!(expr)) {                                                     \
            g_currentPassed = false;                                       \
            g_currentMessage += "\n  EXPECT_TRUE failed: " #expr;         \
        }                                                                  \
    } while (false)

#define EXPECT_FALSE(expr)                                                 \
    do {                                                                   \
        if ((expr)) {                                                      \
            g_currentPassed = false;                                       \
            g_currentMessage += "\n  EXPECT_FALSE failed: " #expr;        \
        }                                                                  \
    } while (false)

#define EXPECT_EQ(a, b)                                                    \
    do {                                                                   \
        if ((a) != (b)) {                                                  \
            g_currentPassed = false;                                       \
            g_currentMessage += "\n  EXPECT_EQ failed: " #a " != " #b;   \
        }                                                                  \
    } while (false)

/// Expects that the statement \p stmt throws an exception of type \p ExType.
/// Both arguments may contain commas, which is safe here because this macro
/// expands inside the lambda body of TEST — the preprocessor has already
/// consumed TEST's arguments before this macro is expanded.
#define EXPECT_THROWS(ExType, stmt)                                        \
    do {                                                                   \
        bool threw_ = false;                                               \
        try { stmt; }                                                      \
        catch (const ExType&) { threw_ = true; }                          \
        catch (...) {}                                                     \
        if (!threw_) {                                                     \
            g_currentPassed = false;                                       \
            g_currentMessage +=                                            \
                "\n  EXPECT_THROWS(" #ExType ", " #stmt ") did not throw.";\
        }                                                                  \
    } while (false)

// ─── Tests ────────────────────────────────────────────────────────────────────

// ── ResourceRequirements ──────────────────────────────────────────────────────

static void test_ResourceRequirements() {

    TEST("ResourceRequirements: valid construction", {
        ResourceRequirements r{4.0, 8.0, 100.0, 1.0};
        EXPECT_EQ(r.cpu,     4.0);
        EXPECT_EQ(r.ram,     8.0);
        EXPECT_EQ(r.storage, 100.0);
        EXPECT_EQ(r.gpu,     1.0);
    });

    TEST("ResourceRequirements: zero values are valid", {
        ResourceRequirements r{0.0, 0.0, 0.0, 0.0};
        EXPECT_TRUE(r.isZero());
    });

    TEST("ResourceRequirements: negative cpu rejected", {
        EXPECT_THROWS(std::invalid_argument, ResourceRequirements(-1.0, 8.0, 100.0, 0.0));
    });

    TEST("ResourceRequirements: negative ram rejected", {
        EXPECT_THROWS(std::invalid_argument, ResourceRequirements(4.0, -1.0, 100.0, 0.0));
    });

    TEST("ResourceRequirements: negative storage rejected", {
        EXPECT_THROWS(std::invalid_argument, ResourceRequirements(4.0, 8.0, -1.0, 0.0));
    });

    TEST("ResourceRequirements: negative gpu rejected", {
        EXPECT_THROWS(std::invalid_argument, ResourceRequirements(4.0, 8.0, 100.0, -1.0));
    });

    TEST("ResourceRequirements: fitsWithin true when all fields fit", {
        ResourceRequirements req{4.0, 8.0, 50.0, 1.0};
        ResourceRequirements avail{16.0, 32.0, 200.0, 4.0};
        EXPECT_TRUE(req.fitsWithin(avail));
    });

    TEST("ResourceRequirements: fitsWithin false when cpu exceeds available", {
        ResourceRequirements req{20.0, 8.0, 50.0, 0.0};
        ResourceRequirements avail{16.0, 32.0, 200.0, 4.0};
        EXPECT_FALSE(req.fitsWithin(avail));
    });

    TEST("ResourceRequirements: fitsWithin false when gpu exceeds available", {
        ResourceRequirements req{4.0, 8.0, 50.0, 5.0};
        ResourceRequirements avail{8.0, 16.0, 100.0, 4.0};
        EXPECT_FALSE(req.fitsWithin(avail));
    });
}

// ── Job ───────────────────────────────────────────────────────────────────────

static void test_Job() {

    TEST("Job: valid construction starts PENDING", {
        ResourceRequirements req{2.0, 4.0, 10.0, 0.0};
        Job j{"j1", req, 1, 0, 5};
        EXPECT_EQ(j.getState(), JobState::PENDING);
        EXPECT_EQ(j.getId(), std::string{"j1"});
        EXPECT_EQ(j.getPriority(), 1);
        EXPECT_EQ(j.getArrivalTick(), 0);
        EXPECT_EQ(j.getDuration(), 5);
    });

    TEST("Job: empty id rejected", {
        ResourceRequirements req{2.0, 4.0, 10.0, 0.0};
        EXPECT_THROWS(std::invalid_argument, Job("", req, 0, 0, 1));
    });

    TEST("Job: negative priority rejected", {
        ResourceRequirements req{2.0, 4.0, 10.0, 0.0};
        EXPECT_THROWS(std::invalid_argument, Job("j", req, -1, 0, 1));
    });

    TEST("Job: negative arrival tick rejected", {
        ResourceRequirements req{2.0, 4.0, 10.0, 0.0};
        EXPECT_THROWS(std::invalid_argument, Job("j", req, 0, -1, 1));
    });

    TEST("Job: zero duration rejected", {
        ResourceRequirements req{2.0, 4.0, 10.0, 0.0};
        EXPECT_THROWS(std::invalid_argument, Job("j", req, 0, 0, 0));
    });

    TEST("Job: negative duration rejected", {
        ResourceRequirements req{2.0, 4.0, 10.0, 0.0};
        EXPECT_THROWS(std::invalid_argument, Job("j", req, 0, 0, -3));
    });

    TEST("Job: normal lifecycle PENDING to COMPLETED", {
        ResourceRequirements req{2.0, 4.0, 10.0, 0.0};
        Job j{"j2", req, 0, 0, 3};
        j.arrive();
        EXPECT_EQ(j.getState(), JobState::QUEUED);
        j.start("srv-1", 0);
        EXPECT_EQ(j.getState(), JobState::RUNNING);
        EXPECT_TRUE(j.getStartTick().has_value());
        EXPECT_EQ(j.getStartTick().value(), 0);
        EXPECT_TRUE(j.getAssignedServerId().has_value());
        j.complete(3);
        EXPECT_EQ(j.getState(), JobState::COMPLETED);
        EXPECT_TRUE(j.getFinishTick().has_value());
        EXPECT_EQ(j.getFinishTick().value(), 3);
        EXPECT_FALSE(j.getAssignedServerId().has_value());
    });

    TEST("Job: cannot arrive twice", {
        ResourceRequirements req{1.0, 1.0, 1.0, 0.0};
        Job j{"j3", req, 0, 0, 1};
        j.arrive();
        EXPECT_THROWS(std::logic_error, j.arrive());
    });

    TEST("Job: cannot start from PENDING", {
        ResourceRequirements req{1.0, 1.0, 1.0, 0.0};
        Job j{"j4", req, 0, 0, 1};
        EXPECT_THROWS(std::logic_error, j.start("srv", 0));
    });

    TEST("Job: cannot start before arrivalTick", {
        ResourceRequirements req{1.0, 1.0, 1.0, 0.0};
        Job j{"j5", req, 0, 5, 1};
        j.arrive();
        EXPECT_THROWS(std::logic_error, j.start("srv", 3));
    });

    TEST("Job: cancel from PENDING", {
        ResourceRequirements req{1.0, 1.0, 1.0, 0.0};
        Job j{"j6", req, 0, 0, 1};
        j.cancel();
        EXPECT_EQ(j.getState(), JobState::CANCELLED);
    });

    TEST("Job: cancel from QUEUED", {
        ResourceRequirements req{1.0, 1.0, 1.0, 0.0};
        Job j{"j7", req, 0, 0, 1};
        j.arrive();
        j.cancel();
        EXPECT_EQ(j.getState(), JobState::CANCELLED);
    });

    TEST("Job: cancel from RUNNING clears assigned server", {
        ResourceRequirements req{1.0, 1.0, 1.0, 0.0};
        Job j{"j8", req, 0, 0, 5};
        j.arrive();
        j.start("srv", 0);
        j.cancel();
        EXPECT_EQ(j.getState(), JobState::CANCELLED);
        EXPECT_FALSE(j.getAssignedServerId().has_value());
    });

    TEST("Job: fail from RUNNING", {
        ResourceRequirements req{1.0, 1.0, 1.0, 0.0};
        Job j{"j9", req, 0, 0, 5};
        j.arrive();
        j.start("srv", 0);
        j.fail();
        EXPECT_EQ(j.getState(), JobState::FAILED);
    });

    TEST("Job: fail from PENDING is rejected", {
        ResourceRequirements req{1.0, 1.0, 1.0, 0.0};
        Job j{"j10", req, 0, 0, 1};
        EXPECT_THROWS(std::logic_error, j.fail());
    });

    TEST("Job: reject from PENDING", {
        ResourceRequirements req{1.0, 1.0, 1.0, 0.0};
        Job j{"j11", req, 0, 0, 1};
        j.reject();
        EXPECT_EQ(j.getState(), JobState::REJECTED);
    });

    TEST("Job: reject from QUEUED", {
        ResourceRequirements req{1.0, 1.0, 1.0, 0.0};
        Job j{"j12", req, 0, 0, 1};
        j.arrive();
        j.reject();
        EXPECT_EQ(j.getState(), JobState::REJECTED);
    });

    TEST("Job: cannot transition out of terminal COMPLETED", {
        ResourceRequirements req{1.0, 1.0, 1.0, 0.0};
        Job j{"j13", req, 0, 0, 1};
        j.arrive();
        j.start("srv", 0);
        j.complete(1);
        EXPECT_THROWS(std::logic_error, j.cancel());
    });

    TEST("Job: cannot arrive from terminal REJECTED", {
        ResourceRequirements req{1.0, 1.0, 1.0, 0.0};
        Job j{"j14", req, 0, 0, 1};
        j.reject();
        EXPECT_THROWS(std::logic_error, j.arrive());
    });
}

// ── Server ────────────────────────────────────────────────────────────────────

static void test_Server() {

    TEST("Server: empty id rejected", {
        ResourceRequirements cap{4.0, 8.0, 100.0, 0.0};
        EXPECT_THROWS(std::invalid_argument, ComputeServer("", cap));
    });

    TEST("Server: initial available equals capacity", {
        ResourceRequirements cap{8.0, 16.0, 200.0, 0.0};
        ComputeServer s{"cs1", cap};
        auto avail = s.getAvailable();
        EXPECT_EQ(avail.cpu,     cap.cpu);
        EXPECT_EQ(avail.ram,     cap.ram);
        EXPECT_EQ(avail.storage, cap.storage);
        EXPECT_EQ(avail.gpu,     cap.gpu);
    });

    TEST("Server: allocated starts at zero", {
        ResourceRequirements cap{8.0, 16.0, 200.0, 0.0};
        ComputeServer s{"cs2", cap};
        auto alloc = s.getAllocated();
        EXPECT_EQ(alloc.cpu,     0.0);
        EXPECT_EQ(alloc.ram,     0.0);
        EXPECT_EQ(alloc.storage, 0.0);
        EXPECT_EQ(alloc.gpu,     0.0);
    });

    TEST("Server: successful reservation reduces available", {
        ResourceRequirements cap{16.0, 32.0, 500.0, 0.0};
        ComputeServer s{"cs3", cap};
        ResourceRequirements req{4.0, 8.0, 50.0, 0.0};
        s.reserveResources(req);
        auto avail = s.getAvailable();
        EXPECT_EQ(avail.cpu,     12.0);
        EXPECT_EQ(avail.ram,     24.0);
        EXPECT_EQ(avail.storage, 450.0);
    });

    TEST("Server: reservation then release restores available", {
        ResourceRequirements cap{16.0, 32.0, 500.0, 0.0};
        ComputeServer s{"cs4", cap};
        ResourceRequirements req{4.0, 8.0, 50.0, 0.0};
        s.reserveResources(req);
        s.releaseResources(req);
        auto avail = s.getAvailable();
        EXPECT_EQ(avail.cpu,     cap.cpu);
        EXPECT_EQ(avail.ram,     cap.ram);
        EXPECT_EQ(avail.storage, cap.storage);
    });

    TEST("Server: over-allocation is rejected atomically", {
        ResourceRequirements cap{4.0, 8.0, 100.0, 0.0};
        ComputeServer s{"cs5", cap};
        ResourceRequirements req{10.0, 8.0, 100.0, 0.0};
        EXPECT_THROWS(std::runtime_error, s.reserveResources(req));
        EXPECT_EQ(s.getAllocated().cpu, 0.0);
        EXPECT_EQ(s.getAllocated().ram, 0.0);
    });

    TEST("Server: releasing more than allocated is rejected", {
        ResourceRequirements cap{8.0, 16.0, 200.0, 0.0};
        ComputeServer s{"cs6", cap};
        ResourceRequirements req{2.0, 4.0, 20.0, 0.0};
        s.reserveResources(req);
        ResourceRequirements tooMuch{4.0, 4.0, 20.0, 0.0};
        EXPECT_THROWS(std::runtime_error, s.releaseResources(tooMuch));
        EXPECT_EQ(s.getAllocated().cpu, 2.0);
    });

    TEST("Server: unavailable server cannot be reserved", {
        ResourceRequirements cap{8.0, 16.0, 200.0, 0.0};
        ComputeServer s{"cs7", cap};
        s.setAvailable(false);
        ResourceRequirements req{1.0, 1.0, 1.0, 0.0};
        EXPECT_THROWS(std::runtime_error, s.reserveResources(req));
    });

    TEST("Server: canAccommodate returns false when unavailable", {
        ResourceRequirements cap{8.0, 16.0, 200.0, 0.0};
        ComputeServer s{"cs8", cap};
        s.setAvailable(false);
        ResourceRequirements req{1.0, 1.0, 1.0, 0.0};
        EXPECT_FALSE(s.canAccommodate(req));
    });

    TEST("Server: multiple reservations respect cumulative accounting", {
        ResourceRequirements cap{8.0, 16.0, 200.0, 0.0};
        ComputeServer s{"cs9", cap};
        ResourceRequirements r1{2.0, 4.0, 50.0, 0.0};
        ResourceRequirements r2{3.0, 8.0, 100.0, 0.0};
        s.reserveResources(r1);
        s.reserveResources(r2);
        EXPECT_EQ(s.getAllocated().cpu, 5.0);
        EXPECT_EQ(s.getAllocated().ram, 12.0);
        // Fill the rest exactly.
        ResourceRequirements r3{3.0, 4.0, 50.0, 0.0};
        s.reserveResources(r3);
        EXPECT_EQ(s.getAvailable().cpu, 0.0);
        // One more must fail.
        ResourceRequirements r4{0.1, 0.0, 0.0, 0.0};
        EXPECT_THROWS(std::runtime_error, s.reserveResources(r4));
    });
}

// ── Server type compatibility ─────────────────────────────────────────────────

static void test_ServerTypeCompatibility() {

    TEST("GPUServer: rejects job with zero GPU", {
        ResourceRequirements cap{8.0, 16.0, 100.0, 4.0};
        GPUServer gs{"gpu1", cap};
        ResourceRequirements cpuOnly{2.0, 4.0, 0.0, 0.0};
        EXPECT_FALSE(gs.canAccommodate(cpuOnly));
        EXPECT_THROWS(std::runtime_error, gs.reserveResources(cpuOnly));
    });

    TEST("GPUServer: accepts job that requests GPUs", {
        ResourceRequirements cap{8.0, 16.0, 100.0, 4.0};
        GPUServer gs{"gpu2", cap};
        ResourceRequirements gpuJob{2.0, 4.0, 10.0, 1.0};
        EXPECT_TRUE(gs.canAccommodate(gpuJob));
        gs.reserveResources(gpuJob);
        EXPECT_EQ(gs.getAllocated().gpu, 1.0);
    });

    TEST("StorageServer: rejects job with zero storage", {
        ResourceRequirements cap{4.0, 8.0, 5000.0, 0.0};
        StorageServer ss{"stor1", cap};
        ResourceRequirements noStorage{1.0, 2.0, 0.0, 0.0};
        EXPECT_FALSE(ss.canAccommodate(noStorage));
        EXPECT_THROWS(std::runtime_error, ss.reserveResources(noStorage));
    });

    TEST("StorageServer: accepts job that requests storage", {
        ResourceRequirements cap{4.0, 8.0, 5000.0, 0.0};
        StorageServer ss{"stor2", cap};
        ResourceRequirements storageJob{1.0, 2.0, 1000.0, 0.0};
        EXPECT_TRUE(ss.canAccommodate(storageJob));
        ss.reserveResources(storageJob);
        EXPECT_EQ(ss.getAllocated().storage, 1000.0);
    });

    TEST("ComputeServer: accepts job with no GPU requirement", {
        ResourceRequirements cap{16.0, 32.0, 0.0, 0.0};
        ComputeServer cs{"comp1", cap};
        ResourceRequirements cpuJob{4.0, 8.0, 0.0, 0.0};
        EXPECT_TRUE(cs.canAccommodate(cpuJob));
    });

    TEST("ComputeServer: rejects job that exceeds its capacity", {
        ResourceRequirements cap{4.0, 8.0, 100.0, 0.0};
        ComputeServer cs{"comp2", cap};
        ResourceRequirements tooBig{8.0, 8.0, 100.0, 0.0};
        EXPECT_FALSE(cs.canAccommodate(tooBig));
    });
}

// ── DataCenter ────────────────────────────────────────────────────────────────

static void test_DataCenter() {

    TEST("DataCenter: starts empty", {
        DataCenter dc;
        EXPECT_EQ(dc.serverCount(), std::size_t{0});
    });

    TEST("DataCenter: addServer increases serverCount", {
        DataCenter dc;
        ResourceRequirements cap{4.0, 8.0, 100.0, 0.0};
        dc.addServer(std::make_unique<ComputeServer>("s1", cap));
        EXPECT_EQ(dc.serverCount(), std::size_t{1});
    });

    TEST("DataCenter: findServer returns correct server", {
        DataCenter dc;
        ResourceRequirements cap{4.0, 8.0, 100.0, 0.0};
        dc.addServer(std::make_unique<ComputeServer>("s2", cap));
        Server* s = dc.findServer("s2");
        EXPECT_TRUE(s != nullptr);
        EXPECT_EQ(s->getId(), std::string{"s2"});
    });

    TEST("DataCenter: findServer returns nullptr for missing id", {
        DataCenter dc;
        EXPECT_TRUE(dc.findServer("nonexistent") == nullptr);
    });

    TEST("DataCenter: duplicate server id is rejected", {
        DataCenter dc;
        ResourceRequirements cap{4.0, 8.0, 100.0, 0.0};
        dc.addServer(std::make_unique<ComputeServer>("dup", cap));
        ResourceRequirements cap2{8.0, 16.0, 200.0, 0.0};
        EXPECT_THROWS(std::invalid_argument,
            dc.addServer(std::make_unique<ComputeServer>("dup", cap2)));
        EXPECT_TRUE(dc.findServer("dup") != nullptr);
        EXPECT_EQ(dc.serverCount(), std::size_t{1});
    });

    TEST("DataCenter: null server pointer is rejected", {
        DataCenter dc;
        EXPECT_THROWS(std::invalid_argument, dc.addServer(nullptr));
    });

    TEST("DataCenter: mixed server types can be added", {
        DataCenter dc;
        ResourceRequirements cap1{16.0, 32.0, 500.0, 0.0};
        ResourceRequirements cap2{8.0,  16.0, 200.0, 4.0};
        ResourceRequirements cap3{4.0,  8.0, 10000.0, 0.0};
        dc.addServer(std::make_unique<ComputeServer>("c1",  cap1));
        dc.addServer(std::make_unique<GPUServer>    ("g1",  cap2));
        dc.addServer(std::make_unique<StorageServer>("st1", cap3));
        EXPECT_EQ(dc.serverCount(), std::size_t{3});
        EXPECT_TRUE(dc.findServer("c1")  != nullptr);
        EXPECT_TRUE(dc.findServer("g1")  != nullptr);
        EXPECT_TRUE(dc.findServer("st1") != nullptr);
    });

    TEST("DataCenter: getServers preserves insertion order", {
        DataCenter dc;
        ResourceRequirements cap{4.0, 8.0, 100.0, 0.0};
        dc.addServer(std::make_unique<ComputeServer>("a", cap));
        dc.addServer(std::make_unique<ComputeServer>("b", cap));
        const auto& servers = dc.getServers();
        EXPECT_EQ(servers.size(), std::size_t{2});
        EXPECT_EQ(servers[0]->getId(), std::string{"a"});
        EXPECT_EQ(servers[1]->getId(), std::string{"b"});
    });
}

// ─── Runner ───────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== Orchestrix Milestone 1 — Automated Tests ===\n\n";

    test_ResourceRequirements();
    test_Job();
    test_Server();
    test_ServerTypeCompatibility();
    test_DataCenter();

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
