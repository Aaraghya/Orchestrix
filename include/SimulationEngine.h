#pragma once

#include "DataCenter.h"
#include "Job.h"
#include "Scheduler.h"
#include "SimulationEvent.h"

#include <deque>
#include <map>
#include <string>
#include <utility>
#include <vector>

/// Drives the discrete-event simulation.
///
/// Responsibilities (architecture.md §3.8, simulation-rules.md §8):
///  - Maintain the simulation clock (integer ticks starting at 0).
///  - Own the job list and the waiting queue.
///  - Enforce the documented tick-processing order.
///  - Reject permanently infeasible jobs and queue temporarily infeasible ones.
///  - Release resources when jobs complete.
///  - Record all meaningful events.
///  - Stop when all jobs are terminal or the maximum-tick guard fires.
///
/// Ownership model:
///  - SimulationEngine owns the jobs (moved in via setWorkload()).
///  - DataCenter and Scheduler are non-owning references; the caller
///    is responsible for their lifetime.
///  - To run the same workload with a different strategy, the caller
///    must reconstruct a fresh DataCenter, Scheduler and SimulationEngine
///    (simulation-rules.md §11).
///
/// This keeps the engine itself free of allocation-strategy logic.
class SimulationEngine {
public:
    static constexpr int DEFAULT_MAX_TICK = 100'000;

    /// Constructs an engine.
    ///
    /// \param scheduler   The Scheduler to use for job placement.
    ///                    Must outlive this engine.
    /// \param dataCenter  The DataCenter owning all servers.
    ///                    Must outlive this engine.
    /// \param maxTick     Hard upper bound on the simulation clock.
    ///                    Prevents infinite loops when jobs stall.
    explicit SimulationEngine(Scheduler& scheduler,
                              DataCenter& dataCenter,
                              int maxTick = DEFAULT_MAX_TICK);

    // Non-copyable; owning the job vector makes copying semantically wrong.
    SimulationEngine(const SimulationEngine&)            = delete;
    SimulationEngine& operator=(const SimulationEngine&) = delete;
    SimulationEngine(SimulationEngine&&)                 = default;
    SimulationEngine& operator=(SimulationEngine&&)      = default;

    // ── Workload ──────────────────────────────────────────────────────────────

    /// Loads the job list.  Must be called before run().
    /// Jobs are moved into the engine; the caller's vector is left empty.
    /// All provided jobs must be in the PENDING state.
    void setWorkload(std::vector<Job> jobs);

    // ── Run ───────────────────────────────────────────────────────────────────

    /// Runs the simulation until all jobs are terminal or maxTick is reached.
    ///
    /// run() may be called only once per engine instance.  To re-run with a
    /// different strategy or fresh state, create a new engine, DataCenter and
    /// Scheduler.
    ///
    /// Throws std::logic_error if called more than once or before setWorkload().
    void run();

    // ── Observers ─────────────────────────────────────────────────────────────

    /// The tick at which the simulation stopped (last processed tick).
    [[nodiscard]] int currentTick()   const noexcept;

    /// Returns true once run() has completed.
    [[nodiscard]] bool hasFinished()  const noexcept;

    /// Whether the run was cut short by the tick limit.
    [[nodiscard]] bool hitMaxTick()   const noexcept;

    /// Read-only access to the job list (for inspecting final states).
    [[nodiscard]] const std::vector<Job>& getJobs() const noexcept;

    /// Read-only access to the event log.
    [[nodiscard]] const std::vector<SimulationEvent>& getEvents() const noexcept;

private:
    // ── State ─────────────────────────────────────────────────────────────────

    Scheduler*   m_scheduler;
    DataCenter*  m_dataCenter;
    int          m_maxTick;
    int          m_currentTick  = 0;
    bool         m_finished     = false;
    bool         m_hitMaxTick   = false;
    bool         m_ran          = false;
    bool         m_workloadSet  = false;

    std::vector<Job>           m_jobs;
    std::deque<Job*>           m_waitQueue;  ///< FIFO queue of QUEUED jobs.

    /// Maps finish_tick → (job*, server*) for all RUNNING jobs.
    /// multimap because multiple jobs may complete at the same tick.
    std::multimap<int, std::pair<Job*, Server*>> m_running;

    std::vector<SimulationEvent> m_events;

    // ── Private helpers ───────────────────────────────────────────────────────

    /// Records one event into the log.
    void logEvent(int tick, EventType type,
                  const std::string& jobId    = "",
                  const std::string& serverId = "",
                  const std::string& detail   = "");

    /// Step 2: complete all running jobs whose finish tick == t.
    void processCompletions(int t);

    /// Step 3–4: process arrivals and mark impossible jobs rejected.
    void processArrivals(int t);

    /// Returns true if the job's requirements can fit on at least one server's
    /// *total* capacity (ignoring current allocation) and the server type
    /// accepts the job.  This is the rejection test (simulation-rules.md §3).
    [[nodiscard]] bool canEverFit(const Job& job) const noexcept;

    /// Step 5–6: run the scheduler over the waiting queue in FIFO order.
    void processScheduling(int t);

    /// Returns true when all jobs are in a terminal state.
    [[nodiscard]] bool allTerminal() const noexcept;

    /// Returns the smallest arrival tick among jobs still in PENDING state,
    /// or INT_MAX if none.
    [[nodiscard]] int nextPendingArrival() const noexcept;

    /// Returns true when there is no pending work and no running jobs
    /// (so the simulation can stop even before maxTick).
    [[nodiscard]] bool noFutureEvents() const noexcept;
};
