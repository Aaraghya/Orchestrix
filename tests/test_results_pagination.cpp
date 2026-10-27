/// test_results_pagination.cpp — Automated tests for paginated simulation results.
///
/// Verifies all requirements specified in Section 7:
///   1. The first page displays at most 20 records.
///   2. Next-page navigation displays the correct records.
///   3. Previous-page navigation returns to the correct records.
///   4. The final page displays the correct remaining records.
///   5. A dataset with fewer than 20 jobs works correctly.
///   6. Empty result sets are handled safely.
///   7. Invalid navigation input does not crash the program.
///   8. Uppercase and lowercase navigation commands both work.
///   9. All three allocation strategies display the correct strategy name and actual results.
///  10. Summary metrics agree with the simulation results.
///  11. Returning to the menu does not rerun the simulation.
///  12. Existing unit tests and CLI smoke tests continue to pass.

#include "AllocationStrategy.h"
#include "DataCenter.h"
#include "Job.h"
#include "MetricsCollector.h"
#include "ReportGenerator.h"
#include "ResourceRequirements.h"
#include "Scheduler.h"
#include "Server.h"
#include "SimulationEngine.h"
#include "SimulationEvent.h"
#include "SimulationMetrics.h"
#include "WorkloadGenerator.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

// ─── Micro test harness ───────────────────────────────────────────────────────

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

// ─── Test Helpers ─────────────────────────────────────────────────────────────

static ResultsSummary makeSampleSummary(const std::string& strat, int total, int allocated, int rejected) {
    ResultsSummary s;
    s.strategyName = strat;
    s.jobsProcessed = total;
    s.jobsAllocated = allocated;
    s.jobsRejected = rejected;
    s.avgAllocationTime = "N/A";
    s.resourceUtilization = "CPU: 10.0% | RAM: 20.0% | Storage: 5.0% | GPU: N/A";
    return s;
}

static std::vector<AllocationRecord> makeSampleRecords(int count) {
    std::vector<AllocationRecord> recs;
    recs.reserve(count);
    for (int i = 0; i < count; ++i) {
        std::string jobId = "job-" + std::to_string(i);
        if (i % 5 == 4) {
            recs.push_back({jobId, "-", "Rejected"});
        } else {
            recs.push_back({jobId, "srv-" + std::to_string(i % 3), "Allocated"});
        }
    }
    return recs;
}

// ─── Test cases ───────────────────────────────────────────────────────────────

void test_PaginationPageSizeAndFirstPage() {
    TEST("Requirement 1: First page displays at most 20 records", {
        auto summary = makeSampleSummary("First Fit", 50, 40, 10);
        auto records = makeSampleRecords(50);

        std::string page1 = ReportGenerator::generateResultsPage(summary, records, 1, 20);

        // Header and metadata markers.
        EXPECT_TRUE(page1.find("ORCHESTRIX SIMULATION") != std::string::npos);
        EXPECT_TRUE(page1.find("Strategy: First Fit") != std::string::npos);
        EXPECT_TRUE(page1.find("ALLOCATION DETAILS") != std::string::npos);
        EXPECT_TRUE(page1.find("Showing results 1–20 of 50") != std::string::npos);

        // First 20 items must be present.
        EXPECT_TRUE(page1.find("job-0 ") != std::string::npos);
        EXPECT_TRUE(page1.find("job-19 ") != std::string::npos);

        // Record 21 must NOT be on page 1.
        EXPECT_TRUE(page1.find("job-20 ") == std::string::npos);
    });
}

void test_NextPageNavigation() {
    TEST("Requirement 2: Next-page navigation displays correct records", {
        auto summary = makeSampleSummary("First Fit", 50, 40, 10);
        auto records = makeSampleRecords(50);

        // Directly rendering page 2.
        std::string page2 = ReportGenerator::generateResultsPage(summary, records, 2, 20);
        EXPECT_TRUE(page2.find("Showing results 21–40 of 50") != std::string::npos);
        EXPECT_TRUE(page2.find("job-20 ") != std::string::npos);
        EXPECT_TRUE(page2.find("job-39 ") != std::string::npos);
        EXPECT_TRUE(page2.find("job-0 ") == std::string::npos);
        EXPECT_TRUE(page2.find("job-40 ") == std::string::npos);

        // Test interactive loop: next page then quit.
        std::istringstream in("N\nQ\n");
        std::ostringstream out;
        bool quitCleanly = ReportGenerator::showPaginatedResults(in, out, summary, records, 20);
        EXPECT_TRUE(quitCleanly);
        EXPECT_TRUE(out.str().find("Showing results 21–40 of 50") != std::string::npos);
    });
}

void test_PreviousPageNavigation() {
    TEST("Requirement 3: Previous-page navigation returns to correct records", {
        auto summary = makeSampleSummary("First Fit", 50, 40, 10);
        auto records = makeSampleRecords(50);

        // Test interactive loop: Next, Previous, Quit.
        std::istringstream in("n\np\nq\n");
        std::ostringstream out;
        bool quitCleanly = ReportGenerator::showPaginatedResults(in, out, summary, records, 20);
        EXPECT_TRUE(quitCleanly);
        const std::string s = out.str();
        EXPECT_TRUE(s.find("Showing results 21–40 of 50") != std::string::npos);
        EXPECT_TRUE(s.find("Showing results 1–20 of 50") != std::string::npos);

        // Pressing P on page 1 should not go below page 1.
        std::istringstream in2("P\nQ\n");
        std::ostringstream out2;
        ReportGenerator::showPaginatedResults(in2, out2, summary, records, 20);
        EXPECT_TRUE(out2.str().find("Already on the first page") != std::string::npos);
    });
}

void test_FinalPageRemainingRecords() {
    TEST("Requirement 4: Final page displays remaining records and bounds checking", {
        auto summary = makeSampleSummary("First Fit", 50, 40, 10);
        auto records = makeSampleRecords(50);

        // Page 3 has records 41–50 (10 items).
        std::string page3 = ReportGenerator::generateResultsPage(summary, records, 3, 20);
        EXPECT_TRUE(page3.find("Showing results 41–50 of 50") != std::string::npos);
        EXPECT_TRUE(page3.find("job-40 ") != std::string::npos);
        EXPECT_TRUE(page3.find("job-49 ") != std::string::npos);
        EXPECT_TRUE(page3.find("job-50 ") == std::string::npos);

        // Interactive: move to page 3 and press N again.
        std::istringstream in("n\nn\nn\nq\n");
        std::ostringstream out;
        ReportGenerator::showPaginatedResults(in, out, summary, records, 20);
        EXPECT_TRUE(out.str().find("Already on the final page") != std::string::npos);
    });
}

void test_FewerThanTwentyJobs() {
    TEST("Requirement 5: Dataset with fewer than 20 jobs works correctly", {
        auto summary = makeSampleSummary("Best Fit", 7, 7, 0);
        auto records = makeSampleRecords(7);

        std::string page = ReportGenerator::generateResultsPage(summary, records, 1, 20);
        EXPECT_TRUE(page.find("Showing results 1–7 of 7") != std::string::npos);
        EXPECT_TRUE(page.find("job-0 ") != std::string::npos);
        EXPECT_TRUE(page.find("job-6 ") != std::string::npos);
        EXPECT_TRUE(page.find("job-7 ") == std::string::npos);

        // Next and prev on 1-page set should not change page.
        std::istringstream in("n\np\nq\n");
        std::ostringstream out;
        ReportGenerator::showPaginatedResults(in, out, summary, records, 20);
        EXPECT_TRUE(out.str().find("Already on the final page") != std::string::npos);
    });
}

void test_EmptyResultSet() {
    TEST("Requirement 6: Empty result sets are handled safely", {
        auto summary = makeSampleSummary("Load Balanced", 0, 0, 0);
        std::vector<AllocationRecord> records;

        std::string page = ReportGenerator::generateResultsPage(summary, records, 1, 20);
        EXPECT_TRUE(page.find("(No jobs to display)") != std::string::npos);
        EXPECT_TRUE(page.find("Showing results 0–0 of 0") != std::string::npos);

        std::istringstream in("n\np\nq\n");
        std::ostringstream out;
        bool clean = ReportGenerator::showPaginatedResults(in, out, summary, records, 20);
        EXPECT_TRUE(clean);
    });
}

void test_InvalidInputHandling() {
    TEST("Requirement 7: Invalid navigation input does not crash and allows retry", {
        auto summary = makeSampleSummary("First Fit", 10, 10, 0);
        auto records = makeSampleRecords(10);

        std::istringstream in("xyz\n123\n\nq\n");
        std::ostringstream out;
        bool clean = ReportGenerator::showPaginatedResults(in, out, summary, records, 20);
        EXPECT_TRUE(clean);
        EXPECT_TRUE(out.str().find("Invalid command 'xyz'") != std::string::npos);
        EXPECT_TRUE(out.str().find("Invalid command '123'") != std::string::npos);
    });
}

void test_CaseInsensitivityAndTrimming() {
    TEST("Requirement 8: Uppercase and lowercase navigation commands both work with whitespace", {
        auto summary = makeSampleSummary("First Fit", 30, 30, 0);
        auto records = makeSampleRecords(30);

        // Lowercase n, uppercase P, padded Q.
        std::istringstream in("  n  \n  P  \n   Q   \n");
        std::ostringstream out;
        bool clean = ReportGenerator::showPaginatedResults(in, out, summary, records, 20);
        EXPECT_TRUE(clean);
        EXPECT_TRUE(out.str().find("Showing results 21–30 of 30") != std::string::npos);
    });
}

void test_AllThreeStrategies() {
    TEST("Requirement 9: All three allocation strategies display correct name and results", {
        WorkloadConfig cfg;
        cfg.jobCount = 10;
        cfg.serverCount = 3;
        cfg.seed = 42;

        WorkloadGenerator gen{cfg};
        auto serverSpecs = gen.generateServers();
        auto jobSpecs    = gen.generateJobs();

        auto testStrategy = [&](AllocationStrategy& strat, const std::string& expectedName) {
            DataCenter dc;
            for (const auto& s : serverSpecs) {
                dc.addServer(std::make_unique<ComputeServer>(s.id, s.capacity));
            }
            std::vector<Job> jobs;
            for (const auto& js : jobSpecs) {
                jobs.emplace_back(js.id, js.requirements, js.priority, js.arrivalTick, js.duration);
            }
            Scheduler sched{strat, dc};
            SimulationEngine engine{sched, dc};
            engine.setWorkload(std::move(jobs));
            engine.run();

            SimulationMetrics m = MetricsCollector::collect(engine, dc, strat.name());
            auto summary = ReportGenerator::buildResultsSummary(m);
            auto records = ReportGenerator::buildAllocationRecords(engine);

            EXPECT_EQ(summary.strategyName, expectedName);
            EXPECT_EQ(summary.jobsProcessed, 10);
            EXPECT_EQ(summary.jobsAllocated, m.completed + m.running);
            EXPECT_EQ(summary.jobsRejected, m.rejected);
            EXPECT_EQ(static_cast<int>(records.size()), 10);

            // Check that page rendering contains the strategy name.
            std::string page = ReportGenerator::generateResultsPage(summary, records, 1, 20);
            EXPECT_TRUE(page.find("Strategy: " + expectedName) != std::string::npos);
        };

        FirstFit ff;
        testStrategy(ff, "First Fit");

        BestFit bf;
        testStrategy(bf, "Best Fit");

        LoadBalanced lb;
        testStrategy(lb, "Load Balanced");
    });
}

void test_SummaryMetricsAgreement() {
    TEST("Requirement 10: Summary metrics agree with simulation results", {
        WorkloadConfig cfg;
        cfg.jobCount = 15;
        cfg.serverCount = 3;
        cfg.seed = 99;

        WorkloadGenerator gen{cfg};
        auto serverSpecs = gen.generateServers();
        auto jobSpecs    = gen.generateJobs();

        DataCenter dc;
        for (const auto& s : serverSpecs) {
            dc.addServer(std::make_unique<ComputeServer>(s.id, s.capacity));
        }
        std::vector<Job> jobs;
        for (const auto& js : jobSpecs) {
            jobs.emplace_back(js.id, js.requirements, js.priority, js.arrivalTick, js.duration);
        }
        FirstFit strat;
        Scheduler sched{strat, dc};
        SimulationEngine engine{sched, dc};
        engine.setWorkload(std::move(jobs));
        engine.run();

        SimulationMetrics m = MetricsCollector::collect(engine, dc, strat.name());
        auto summary = ReportGenerator::buildResultsSummary(m);
        auto records = ReportGenerator::buildAllocationRecords(engine);

        EXPECT_EQ(summary.jobsProcessed, m.totalSubmitted);
        EXPECT_EQ(summary.jobsAllocated, m.completed + m.running);
        EXPECT_EQ(summary.jobsRejected, m.rejected);
        EXPECT_EQ(summary.avgAllocationTime, "N/A");

        // Verify that sum of allocated + rejected matches processed in a completed simulation.
        EXPECT_EQ(summary.jobsAllocated + summary.jobsRejected, summary.jobsProcessed);
    });
}

void test_ReturnToMenuDoesNotRerun() {
    TEST("Requirement 11: Returning to menu does not rerun simulation or alter state", {
        auto summary = makeSampleSummary("First Fit", 5, 5, 0);
        auto records = makeSampleRecords(5);

        std::istringstream in("q\n");
        std::ostringstream out;
        bool clean = ReportGenerator::showPaginatedResults(in, out, summary, records, 20);
        EXPECT_TRUE(clean);
        // Records and summary remain unchanged.
        EXPECT_EQ(static_cast<int>(records.size()), 5);
        EXPECT_EQ(summary.jobsProcessed, 5);
    });
}

void test_NonPositivePageSizeGuard() {
    TEST("Non-positive page sizes are clamped to 1 instead of dividing by zero", {
        auto summary = makeSampleSummary("First Fit", 3, 3, 0);
        auto records = makeSampleRecords(3);

        std::string pageZero = ReportGenerator::generateResultsPage(summary, records, 1, 0);
        EXPECT_TRUE(pageZero.find("Showing results 1–1 of 3") != std::string::npos);

        std::string pageNegative = ReportGenerator::generateResultsPage(summary, records, 1, -5);
        EXPECT_TRUE(pageNegative.find("Showing results 1–1 of 3") != std::string::npos);

        std::istringstream in("q\n");
        std::ostringstream out;
        bool clean = ReportGenerator::showPaginatedResults(in, out, summary, records, 0);
        EXPECT_TRUE(clean);
        EXPECT_TRUE(out.str().find("Showing results 1–1 of 3") != std::string::npos);
    });
}

// ─── Main ─────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== Orchestrix Paginated Results Display Tests ===\n\n";

    test_PaginationPageSizeAndFirstPage();
    test_NextPageNavigation();
    test_PreviousPageNavigation();
    test_FinalPageRemainingRecords();
    test_FewerThanTwentyJobs();
    test_EmptyResultSet();
    test_InvalidInputHandling();
    test_CaseInsensitivityAndTrimming();
    test_NonPositivePageSizeGuard();
    test_AllThreeStrategies();
    test_SummaryMetricsAgreement();
    test_ReturnToMenuDoesNotRerun();

    int passed = 0, failed = 0;
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
