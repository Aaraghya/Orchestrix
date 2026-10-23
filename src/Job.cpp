#include "Job.h"
#include <stdexcept>

// ─── Free function ────────────────────────────────────────────────────────────

std::string jobStateToString(JobState state) {
    switch (state) {
        case JobState::PENDING:   return "PENDING";
        case JobState::QUEUED:    return "QUEUED";
        case JobState::RUNNING:   return "RUNNING";
        case JobState::COMPLETED: return "COMPLETED";
        case JobState::REJECTED:  return "REJECTED";
        case JobState::CANCELLED: return "CANCELLED";
        case JobState::FAILED:    return "FAILED";
    }
    return "UNKNOWN";
}

// ─── Constructor ──────────────────────────────────────────────────────────────

Job::Job(std::string id,
         ResourceRequirements requirements,
         int priority,
         int arrivalTick,
         int duration)
    : m_id{std::move(id)}
    , m_requirements{std::move(requirements)}
    , m_priority{priority}
    , m_arrivalTick{arrivalTick}
    , m_duration{duration}
    , m_state{JobState::PENDING}
{
    // Validate the ID.
    if (m_id.empty()) {
        throw std::invalid_argument("Job: id must not be empty.");
    }

    // Priority must be non-negative so that the field has a consistent meaning.
    // Higher values mean higher urgency; zero is the baseline.
    if (m_priority < 0) {
        throw std::invalid_argument(
            "Job '" + m_id + "': priority must be non-negative, got " +
            std::to_string(m_priority));
    }

    // simulation-rules.md §2: "each job has an arrival tick".
    // Tick 0 is the earliest possible arrival.
    if (m_arrivalTick < 0) {
        throw std::invalid_argument(
            "Job '" + m_id + "': arrivalTick must be non-negative, got " +
            std::to_string(m_arrivalTick));
    }

    // simulation-rules.md §9: "a positive integer execution duration".
    if (m_duration < 1) {
        throw std::invalid_argument(
            "Job '" + m_id + "': duration must be at least 1, got " +
            std::to_string(m_duration));
    }
}

// ─── Accessors ────────────────────────────────────────────────────────────────

const std::string& Job::getId() const noexcept { return m_id; }

const ResourceRequirements& Job::getRequirements() const noexcept {
    return m_requirements;
}

int Job::getPriority()    const noexcept { return m_priority; }
int Job::getArrivalTick() const noexcept { return m_arrivalTick; }
int Job::getDuration()    const noexcept { return m_duration; }
JobState Job::getState()  const noexcept { return m_state; }

std::optional<int> Job::getStartTick()  const noexcept { return m_startTick; }
std::optional<int> Job::getFinishTick() const noexcept { return m_finishTick; }

std::optional<std::string> Job::getAssignedServerId() const noexcept {
    return m_assignedServerId;
}

// ─── State transitions ────────────────────────────────────────────────────────

void Job::arrive() {
    requireState(JobState::PENDING, "arrive");
    m_state = JobState::QUEUED;
}

void Job::start(const std::string& serverId, int startTick) {
    requireState(JobState::QUEUED, "start");

    if (serverId.empty()) {
        throw std::invalid_argument(
            "Job '" + m_id + "': serverId must not be empty in start().");
    }
    if (startTick < m_arrivalTick) {
        // simulation-rules.md §2: "a job must never start before its arrival tick."
        throw std::logic_error(
            "Job '" + m_id + "': cannot start at tick " +
            std::to_string(startTick) + " before arrival tick " +
            std::to_string(m_arrivalTick) + ".");
    }

    m_state            = JobState::RUNNING;
    m_assignedServerId = serverId;
    m_startTick        = startTick;
}

void Job::complete(int finishTick) {
    requireState(JobState::RUNNING, "complete");

    // Sanity check: finish must be after start.
    if (m_startTick.has_value() && finishTick < *m_startTick) {
        throw std::logic_error(
            "Job '" + m_id + "': finishTick " + std::to_string(finishTick) +
            " is earlier than startTick " + std::to_string(*m_startTick) + ".");
    }

    m_state            = JobState::COMPLETED;
    m_finishTick       = finishTick;
    m_assignedServerId = std::nullopt;
}

void Job::cancel() {
    // PENDING, QUEUED or RUNNING → CANCELLED.
    if (m_state != JobState::PENDING &&
        m_state != JobState::QUEUED  &&
        m_state != JobState::RUNNING) {
        throw std::logic_error(
            "Job '" + m_id + "': cannot cancel from state " +
            jobStateToString(m_state) + ".");
    }
    m_state            = JobState::CANCELLED;
    m_assignedServerId = std::nullopt;
}

void Job::fail() {
    requireState(JobState::RUNNING, "fail");
    m_state            = JobState::FAILED;
    m_assignedServerId = std::nullopt;
}

void Job::reject() {
    if (m_state != JobState::PENDING && m_state != JobState::QUEUED) {
        throw std::logic_error(
            "Job '" + m_id + "': cannot reject from state " +
            jobStateToString(m_state) + ".");
    }
    m_state = JobState::REJECTED;
}

// ─── Private helpers ──────────────────────────────────────────────────────────

void Job::requireState(JobState expected, const std::string& operation) const {
    if (m_state != expected) {
        throw std::logic_error(
            "Job '" + m_id + "': operation '" + operation +
            "' requires state " + jobStateToString(expected) +
            " but current state is " + jobStateToString(m_state) + ".");
    }
}

void Job::requireNonTerminal(const std::string& operation) const {
    if (isTerminal()) {
        throw std::logic_error(
            "Job '" + m_id + "': operation '" + operation +
            "' is not allowed from terminal state " +
            jobStateToString(m_state) + ".");
    }
}

bool Job::isTerminal() const noexcept {
    return m_state == JobState::COMPLETED ||
           m_state == JobState::REJECTED  ||
           m_state == JobState::CANCELLED ||
           m_state == JobState::FAILED;
}
