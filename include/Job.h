#pragma once

#include "ResourceRequirements.h"
#include <string>
#include <optional>
#include <stdexcept>

/// All possible states a job can be in during the simulation.
///
/// Documented in simulation-rules.md §5.
/// Terminal states: REJECTED, COMPLETED, CANCELLED, FAILED.
enum class JobState {
    PENDING,    ///< Arrival tick has not been reached yet.
    QUEUED,     ///< Arrived and waiting for a free server.
    RUNNING,    ///< Allocated to a server and executing.
    COMPLETED,  ///< Execution finished successfully.
    REJECTED,   ///< Cannot fit on any compatible server.
    CANCELLED,  ///< Explicitly cancelled before or during execution.
    FAILED      ///< Execution terminated unsuccessfully.
};

/// Returns a human-readable name for a JobState.
[[nodiscard]] std::string jobStateToString(JobState state);

/// Represents a computational workload submitted to the simulator.
///
/// Design decisions:
///  - Job ID is a non-empty string; the caller is responsible for uniqueness
///    at the DataCenter level (just as server-ID uniqueness lives in DataCenter).
///  - Priority is a non-negative integer; higher values mean higher urgency.
///    The base scheduler uses FIFO, but the field is present for later milestones.
///  - arrivalTick >= 0, duration >= 1 (simulation-rules.md §2, §9).
///  - State transitions are enforced by the class; illegal moves throw
///    std::logic_error so bugs surface immediately rather than silently
///    corrupting the simulation.
///  - assignedServerId is set when the job starts and cleared if the job
///    leaves the RUNNING state before completion (cancelled/failed).
class Job {
public:
    /// Constructs a Job and validates all invariants.
    ///
    /// \param id           Non-empty job identifier.
    /// \param requirements Resource requirements (validated internally).
    /// \param priority     Non-negative integer priority.
    /// \param arrivalTick  Non-negative tick at which the job arrives.
    /// \param duration     Positive number of ticks the job runs for.
    Job(std::string id,
        ResourceRequirements requirements,
        int priority,
        int arrivalTick,
        int duration);

    // ── Accessors ──────────────────────────────────────────────────────────

    [[nodiscard]] const std::string&        getId()           const noexcept;
    [[nodiscard]] const ResourceRequirements& getRequirements() const noexcept;
    [[nodiscard]] int                        getPriority()     const noexcept;
    [[nodiscard]] int                        getArrivalTick()  const noexcept;
    [[nodiscard]] int                        getDuration()     const noexcept;
    [[nodiscard]] JobState                   getState()        const noexcept;

    /// The tick at which the job was started (only valid in RUNNING/COMPLETED/FAILED/CANCELLED-after-start).
    [[nodiscard]] std::optional<int>         getStartTick()    const noexcept;

    /// The tick at which the job finished (only valid in COMPLETED).
    [[nodiscard]] std::optional<int>         getFinishTick()   const noexcept;

    /// The server this job is running on (only set while RUNNING).
    [[nodiscard]] std::optional<std::string> getAssignedServerId() const noexcept;

    // ── State transitions ──────────────────────────────────────────────────
    // Each transition validates the current state before applying.
    // Illegal transitions throw std::logic_error.

    /// PENDING → QUEUED  (called when the job's arrival tick is reached).
    void arrive();

    /// QUEUED → RUNNING  (called when a server is allocated).
    void start(const std::string& serverId, int startTick);

    /// RUNNING → COMPLETED  (called by the engine at the finish tick).
    void complete(int finishTick);

    /// PENDING|QUEUED|RUNNING → CANCELLED
    void cancel();

    /// RUNNING → FAILED
    void fail();

    /// PENDING|QUEUED → REJECTED  (called when no compatible server exists).
    void reject();

private:
    std::string             m_id;
    ResourceRequirements    m_requirements;
    int                     m_priority;
    int                     m_arrivalTick;
    int                     m_duration;
    JobState                m_state;
    std::optional<int>      m_startTick;
    std::optional<int>      m_finishTick;
    std::optional<std::string> m_assignedServerId;

    /// Throws std::logic_error with a descriptive message if the transition is
    /// not allowed from the current state.
    void requireState(JobState expected, const std::string& operation) const;

    /// Throws if m_state is already terminal.
    void requireNonTerminal(const std::string& operation) const;

    [[nodiscard]] bool isTerminal() const noexcept;
};
