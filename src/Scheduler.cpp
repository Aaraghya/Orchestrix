#include "Scheduler.h"
#include <stdexcept>
#include <string>

Scheduler::Scheduler(const AllocationStrategy& strategy, DataCenter& dataCenter)
    : m_strategy{&strategy}
    , m_dataCenter{&dataCenter}
{}

Server* Scheduler::scheduleJob(Job& job, int currentTick) {
    // ── Pre-condition checks ──────────────────────────────────────────────────

    if (job.getState() != JobState::QUEUED) {
        throw std::logic_error(
            "Scheduler::scheduleJob: job '" + job.getId() +
            "' must be in QUEUED state, but is in " +
            jobStateToString(job.getState()) + ".");
    }

    if (currentTick < job.getArrivalTick()) {
        // simulation-rules.md §2: "a job must never start before its arrival tick."
        throw std::logic_error(
            "Scheduler::scheduleJob: currentTick " +
            std::to_string(currentTick) +
            " is before arrivalTick " +
            std::to_string(job.getArrivalTick()) +
            " for job '" + job.getId() + "'.");
    }

    // ── Strategy selection ────────────────────────────────────────────────────

    Server* chosen = m_strategy->select(
        m_dataCenter->getServers(), job.getRequirements());

    if (chosen == nullptr) {
        // No feasible server at this moment; job stays QUEUED.
        // No server state has been touched.
        return nullptr;
    }

    // ── Resource reservation ──────────────────────────────────────────────────
    // reserveResources() throws on failure (which would be a logic error here
    // since canAccommodate() already returned true inside the strategy).
    // We propagate any such exception without masking it.

    chosen->reserveResources(job.getRequirements());

    // ── Job state transition ──────────────────────────────────────────────────

    job.start(chosen->getId(), currentTick);

    return chosen;
}

std::string Scheduler::strategyName() const {
    return m_strategy->name();
}

void Scheduler::setStrategy(const AllocationStrategy& strategy) noexcept {
    m_strategy = &strategy;
}
