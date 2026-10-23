#pragma once

#include <optional>
#include <string>

/// All metrics produced from a completed simulation run.
///
/// Metric formulas and denominators
/// ─────────────────────────────────
///
/// Job outcome rates
///   completionRate = (completed / totalSubmitted) * 100  [%]
///   rejectionRate  = (rejected  / totalSubmitted) * 100  [%]
///   Denominator: totalSubmitted.  Both rates are nullopt when totalSubmitted == 0.
///
/// Waiting time
///   For each job j that started:  waitingTime_j = startTick_j - arrivalTick_j
///   avgWaitingTime = sum(waitingTime_j) / count(started jobs)
///   Denominator: number of jobs that actually started (not total submitted).
///   Reported as nullopt when no job ever started.
///
/// Throughput
///   throughput = completed / elapsedTicks
///   Denominator: elapsedTicks (== engine.currentTick()).
///   Reported as nullopt when elapsedTicks == 0.
///
/// Resource utilization (per dimension)
///   For each started job j placed on server s, occupying ticks [startTick, finishTick):
///     allocatedTime_dim += req_dim * (finishTick - startTick)
///   For each server s:
///     totalTime_dim += capacity_dim * elapsedTicks
///   utilization_dim = sum(allocatedTime_dim) / sum(totalTime_dim)
///   Reported as nullopt when the total available resource-time for that
///   dimension is zero (no server has positive capacity for it, or elapsedTicks==0).
struct SimulationMetrics {
    // ── Identity ──────────────────────────────────────────────────────────────
    std::string strategyName;
    int         elapsedTicks = 0;   ///< engine.currentTick() at stop.
    bool        hitMaxTick   = false;
    bool        finishedNormally = false;

    // ── Job outcomes ──────────────────────────────────────────────────────────
    int totalSubmitted = 0;
    int completed      = 0;
    int rejected       = 0;
    int cancelled      = 0;
    int failed         = 0;
    int stillActive    = 0;   ///< QUEUED or RUNNING when simulation stopped.

    /// completed / totalSubmitted * 100.  nullopt when totalSubmitted == 0.
    std::optional<double> completionRate;
    /// rejected / totalSubmitted * 100.  nullopt when totalSubmitted == 0.
    std::optional<double> rejectionRate;

    // ── Waiting time ──────────────────────────────────────────────────────────
    /// Average of (startTick - arrivalTick) over jobs that actually started.
    /// nullopt when no job started.
    std::optional<double> avgWaitingTime;
    int waitingTimeSampleCount = 0;   ///< Number of jobs in the average.

    // ── Throughput ────────────────────────────────────────────────────────────
    /// completed / elapsedTicks.  nullopt when elapsedTicks == 0.
    std::optional<double> throughput;

    // ── Resource utilization (resource-time accounting) ───────────────────────
    /// sum(req.cpu * duration) / sum(cap.cpu * elapsedTicks) across all servers.
    /// nullopt when no server has positive CPU capacity or elapsedTicks == 0.
    std::optional<double> cpuUtilization;
    /// Same formula for RAM.
    std::optional<double> ramUtilization;
    /// Same formula for storage.
    std::optional<double> storageUtilization;
    /// Same formula for GPU.
    std::optional<double> gpuUtilization;
};
