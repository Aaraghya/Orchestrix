#include "SimulationEngine.h"
#include "Server.h"

#include <climits>
#include <stdexcept>
#include <string>

// ─── Constructor ──────────────────────────────────────────────────────────────

SimulationEngine::SimulationEngine(Scheduler& scheduler,
                                   DataCenter& dataCenter,
                                   int maxTick)
    : m_scheduler{&scheduler}
    , m_dataCenter{&dataCenter}
    , m_maxTick{maxTick}
{
    if (maxTick < 0) {
        throw std::invalid_argument(
            "SimulationEngine: maxTick must be non-negative, got " +
            std::to_string(maxTick));
    }
}

// ── Workload ──────────────────────────────────────────────────────────────────

void SimulationEngine::setWorkload(std::vector<Job> jobs) {
    if (m_ran) {
        throw std::logic_error(
            "SimulationEngine::setWorkload: cannot set workload after run().");
    }
    // Verify every job is PENDING.
    for (const auto& j : jobs) {
        if (j.getState() != JobState::PENDING) {
            throw std::invalid_argument(
                "SimulationEngine::setWorkload: job '" + j.getId() +
                "' is not in PENDING state.");
        }
    }
    m_jobs        = std::move(jobs);
    m_workloadSet = true;
}

// ── Run ───────────────────────────────────────────────────────────────────────

void SimulationEngine::run() {
    if (m_ran) {
        throw std::logic_error(
            "SimulationEngine::run: already ran; create a new engine for a fresh run.");
    }
    if (!m_workloadSet) {
        throw std::logic_error(
            "SimulationEngine::run: call setWorkload() before run().");
    }
    m_ran = true;

    logEvent(0, EventType::SIM_STARTED, "", "", "Simulation started");

    // If there are no jobs at all, finish immediately at tick 0.
    if (m_jobs.empty()) {
        m_currentTick = 0;
        m_finished    = true;
        logEvent(0, EventType::SIM_FINISHED, "", "", "No jobs; simulation complete");
        return;
    }

    // ── Main tick loop ────────────────────────────────────────────────────────
    //
    // We advance the clock only to ticks where something can happen:
    //   - A running job completes.
    //   - A pending job arrives.
    //   - There are queued jobs to try scheduling.
    //
    // This avoids spinning through empty ticks while still being correct.
    // However, to keep the implementation simple and auditable, we use a
    // straightforward tick-by-tick loop that breaks when there is no more
    // work.  The max-tick guard prevents pathological cases.

    for (m_currentTick = 0; m_currentTick <= m_maxTick; ++m_currentTick) {
        const int t = m_currentTick;

        // ── Step 1: clock already advanced by the loop counter.

        // ── Step 2: complete running jobs whose finish tick is t.
        processCompletions(t);

        // ── Step 3–4: process arrivals, mark impossible jobs REJECTED.
        processArrivals(t);

        // ── Step 5–6: schedule queued jobs.
        processScheduling(t);

        // ── Step 7: events already recorded inline.

        // ── Step 8: check termination.
        if (allTerminal()) {
            m_finished = true;
            logEvent(t, EventType::SIM_FINISHED, "", "",
                     "All jobs terminal at tick " + std::to_string(t));
            return;
        }

        // If there are no future events we can stop early without hitting
        // maxTick — this handles stalled queues (permanent deadlock).
        if (noFutureEvents()) {
            m_finished = true;
            logEvent(t, EventType::SIM_FINISHED, "", "",
                     "No future events; simulation complete at tick " +
                     std::to_string(t));
            return;
        }
    }

    // Reached maxTick without all jobs finishing.
    m_hitMaxTick = true;
    m_finished   = true;
    logEvent(m_maxTick, EventType::SIM_MAX_TICK, "", "",
             "Tick limit " + std::to_string(m_maxTick) + " reached");
}

// ── Observers ─────────────────────────────────────────────────────────────────

int  SimulationEngine::currentTick()  const noexcept { return m_currentTick; }
bool SimulationEngine::hasFinished()  const noexcept { return m_finished;    }
bool SimulationEngine::hitMaxTick()   const noexcept { return m_hitMaxTick;  }

const std::vector<Job>& SimulationEngine::getJobs() const noexcept {
    return m_jobs;
}

const std::vector<SimulationEvent>& SimulationEngine::getEvents() const noexcept {
    return m_events;
}

// ── Private helpers ───────────────────────────────────────────────────────────

void SimulationEngine::logEvent(int tick, EventType type,
                                const std::string& jobId,
                                const std::string& serverId,
                                const std::string& detail) {
    m_events.push_back({tick, type, jobId, serverId, detail});
}

void SimulationEngine::processCompletions(int t) {
    // Extract all entries whose finish tick == t.
    auto range = m_running.equal_range(t);
    for (auto it = range.first; it != range.second; ++it) {
        Job*    job    = it->second.first;
        Server* server = it->second.second;

        // Release resources before transitioning the job state.
        // (simulation-rules.md §4: release must correspond to a valid reservation)
        server->releaseResources(job->getRequirements());
        logEvent(t, EventType::RESOURCE_RELEASED, job->getId(), server->getId(),
                 "Released resources at completion");

        job->complete(t);
        logEvent(t, EventType::JOB_COMPLETED, job->getId(), server->getId());
    }
    m_running.erase(range.first, range.second);
}

void SimulationEngine::processArrivals(int t) {
    for (auto& job : m_jobs) {
        if (job.getState() != JobState::PENDING) {
            continue;
        }
        if (job.getArrivalTick() != t) {
            continue;
        }

        // Transition: PENDING → QUEUED
        job.arrive();
        logEvent(t, EventType::JOB_ARRIVED, job.getId());

        // Rejection test: can this job ever fit on any compatible server?
        // We check total capacity (not available) so a temporarily full server
        // does not cause a feasible job to be permanently rejected.
        if (!canEverFit(job)) {
            job.reject();
            logEvent(t, EventType::JOB_REJECTED, job.getId(), "",
                     "No compatible server has sufficient total capacity");
        } else {
            // Add to the back of the FIFO wait queue.
            m_waitQueue.push_back(&job);
        }
    }
}

bool SimulationEngine::canEverFit(const Job& job) const noexcept {
    const ResourceRequirements& req = job.getRequirements();
    for (const auto& sptr : m_dataCenter->getServers()) {
        const Server& s = *sptr;
        // canRunJob() is protected; we use the public interface:
        // A server that can accommodate a zero-allocated version of itself
        // would accept this job if it were completely free.  We approximate
        // this by checking fitsWithin(capacity) AND canAccommodate on a
        // temporarily-reset view — but since we can't reset a live server,
        // we instead check: does req fit within total capacity?
        //
        // We also need the compatibility check (canRunJob).  That is exposed
        // indirectly: if all capacity were available, canAccommodate() would
        // return canRunJob() && req.fitsWithin(capacity).  We can simulate
        // this by temporarily noting: if the server is completely available
        // (all resources free), canAccommodate(req) == canRunJob(req) &&
        // req.fitsWithin(cap).  We cannot call canRunJob directly (protected),
        // but we CAN call canAccommodate on a server that has zero allocation.
        //
        // The simplest correct approach: check req.fitsWithin(capacity) for
        // the total-capacity check, then confirm the type compatibility by
        // calling canAccommodate on the server — if the server is empty and
        // the req fits in capacity, canAccommodate must return true.
        // If the server is currently partially or fully allocated we cannot use
        // canAccommodate directly for the feasibility test.
        //
        // Solution: check fitsWithin(capacity) AND isAvailable() AND
        // use a proxy call.  We test isAvailable separately and call
        // canAccommodate only to get the type-compatibility answer —
        // but canAccommodate also checks available resources, so we
        // need a workaround.
        //
        // Best approach without modifying the Server API:
        //   The canRunJob check is a *type* property, not a capacity property.
        //   GPUServer rejects jobs with gpu == 0; StorageServer rejects jobs
        //   with storage == 0.  We can determine this implicitly:
        //   if req.fitsWithin(cap) and the server is currently fresh (not allocated),
        //   canAccommodate == true.  But we don't want to temporarily clear state.
        //
        // Chosen approach:
        //   We add a public helper `canRunJob_public` that wraps the protected
        //   virtual — but that would require a change to the Server header.
        //
        // Alternative (chosen here because it avoids API changes):
        //   We use the fact that canAccommodate = canRunJob && fitsWithin(avail).
        //   We temporarily check: would this server accommodate the job if it
        //   were given *any* feasible available vector?  The only way to know
        //   canRunJob without exposing it is to call canAccommodate with a
        //   request that definitely fits the available capacity.  But we're
        //   here precisely because capacity might be fully used.
        //
        //   The cleanest design-preserving solution: expose a public
        //   `isCompatibleWith(req)` on Server that calls canRunJob().
        //   We add that now as a non-breaking addition.
        //
        // NOTE: We will add Server::isCompatibleWith() to the Server class.
        // That is a small, non-breaking, backwards-compatible addition.
        // All existing tests remain valid.

        if (req.fitsWithin(s.getCapacity()) && s.isCompatibleWith(req)) {
            return true;
        }
    }
    return false;
}

void SimulationEngine::processScheduling(int t) {
    // FIFO scan of the wait queue.
    // "A job that cannot fit must not permanently block other jobs"
    // (simulation-rules.md §7): we use a full-scan approach.
    // Jobs that cannot be scheduled this tick stay in the queue.

    std::deque<Job*> stillWaiting;

    while (!m_waitQueue.empty()) {
        Job* job = m_waitQueue.front();
        m_waitQueue.pop_front();

        // scheduleJob() returns the server on success, nullptr on failure.
        Server* placed = m_scheduler->scheduleJob(*job, t);

        if (placed != nullptr) {
            // Compute finish tick and register in the running map.
            int finishTick = t + job->getDuration();
            m_running.emplace(finishTick, std::make_pair(job, placed));
            logEvent(t, EventType::JOB_STARTED, job->getId(), placed->getId(),
                     "Finish tick=" + std::to_string(finishTick));
        } else {
            // Job stays QUEUED; try again next tick.
            stillWaiting.push_back(job);
        }
    }

    m_waitQueue = std::move(stillWaiting);
}

bool SimulationEngine::allTerminal() const noexcept {
    for (const auto& job : m_jobs) {
        const JobState s = job.getState();
        if (s != JobState::COMPLETED &&
            s != JobState::REJECTED  &&
            s != JobState::CANCELLED &&
            s != JobState::FAILED) {
            return false;
        }
    }
    return true;
}

int SimulationEngine::nextPendingArrival() const noexcept {
    int earliest = INT_MAX;
    for (const auto& job : m_jobs) {
        if (job.getState() == JobState::PENDING) {
            earliest = std::min(earliest, job.getArrivalTick());
        }
    }
    return earliest;
}

bool SimulationEngine::noFutureEvents() const noexcept {
    // There are future events if:
    //   a) any job is still PENDING (will arrive later), or
    //   b) any job is RUNNING (will complete later), or
    //   c) any job is QUEUED (waiting in the queue — though if
    //      the queue is non-empty and no server can ever fit them,
    //      those jobs should have been REJECTED already).
    for (const auto& job : m_jobs) {
        const JobState s = job.getState();
        if (s == JobState::PENDING ||
            s == JobState::RUNNING ||
            s == JobState::QUEUED) {
            return false;
        }
    }
    return true;
}
