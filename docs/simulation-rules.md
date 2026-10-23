# Orchestrix — Simulation Rules

## 1. Purpose

This document defines the rules governing simulated time, job arrivals, resource allocation, job execution, completion and performance measurement.

All components must follow these rules consistently.

## 2. Simulation Time

- Simulation time is represented by a non-negative integer tick.
- The simulation starts at tick 0.
- Time advances in increments of exactly one tick.
- Each job has an arrival tick and a positive integer execution duration.
- No real-world waiting or sleeping is required. Simulation time advances through program logic.
- A job must never start before its arrival tick.

## 3. Job Arrival

- Jobs become eligible for scheduling at their specified arrival tick.
- Jobs with future arrival ticks remain pending.
- When a job arrives, its requirements are validated against the total capacities of compatible servers.
- A job that cannot fit on any compatible server is marked REJECTED.
- A job that can fit on at least one compatible server but cannot currently be allocated remains QUEUED.
- A temporarily full server must not cause an otherwise feasible job to be rejected.

## 4. Server Resources

Each server has four resource capacities:

- CPU: number of cores.
- RAM: GB.
- Storage: GB.
- GPU: number of devices.

Each job specifies the amount of each resource it requires.

For every resource dimension:

Available capacity = Total capacity - Allocated capacity.

The following invariants must always hold:

- Available capacity is never negative.
- Allocated capacity never exceeds total capacity.
- Available capacity plus allocated capacity equals total capacity.
- Resource reservation either succeeds completely or makes no changes.
- Resource release must correspond to a valid, existing reservation.
- The same reservation must never be released more than once.

A server may accommodate a job only if it satisfies all of the job's resource requirements and workload compatibility constraints.

## 5. Job Lifecycle

Supported states:

- PENDING: arrival tick has not been reached.
- QUEUED: arrived and waiting for allocation.
- RUNNING: allocated to a server and executing.
- COMPLETED: execution finished successfully.
- REJECTED: impossible to allocate under the configured server capabilities.
- CANCELLED: execution or waiting was cancelled.
- FAILED: execution terminated unsuccessfully.

Valid normal transitions:

PENDING -> QUEUED -> RUNNING -> COMPLETED

Additional transitions:

- PENDING -> CANCELLED
- QUEUED -> CANCELLED
- RUNNING -> CANCELLED
- RUNNING -> FAILED

REJECTED, COMPLETED, CANCELLED and FAILED are terminal states in the initial implementation.

Invalid state transitions must be prevented.

## 6. Scheduling Strategies

The initial implementation supports First Fit, Best Fit and Load Balanced.

### 6.1 First Fit

- Examine servers in their configured order.
- Select the first compatible server with sufficient available resources.
- Return no candidate if no server qualifies.

### 6.2 Best Fit

- Consider only compatible servers with sufficient available resources.
- Estimate the remaining resource capacity after placing the job.
- Normalize remaining capacity against each server's total capacity for the resource dimensions required by the job.
- Select the feasible server with the smallest sum of normalized remaining capacities.
- Exclude resource dimensions not required by the job.
- Break ties deterministically using server ID.

### 6.3 Load Balanced

- Consider only compatible servers with sufficient available resources.
- Estimate each candidate's resource utilization after placing the job.
- Calculate the average normalized utilization across the server's resource dimensions with positive total capacity.
- Select the candidate with the lowest projected average utilization.
- Break ties deterministically using server ID.

The scheduler must not allocate a job when the selected strategy returns no candidate. The ResourceManager must validate and reserve resources before execution begins.

## 7. Scheduling Order and Queueing

- The waiting queue initially follows FIFO order.
- At each tick, the scheduler examines waiting jobs in queue order.
- If a job cannot be allocated, it remains queued.
- The scheduler may continue examining later jobs in the same tick.
- A job that cannot fit must not permanently block other jobs that could run.
- Newly arrived jobs may start at their arrival tick if resources are available.
- Scheduling is performed after the tick's completion and arrival events have been processed.

Priority scheduling and starvation prevention are optional extensions. They must not silently alter the initial FIFO scheduling rules.

## 8. Tick Processing Order

At each tick t, process events in this order:

1. Advance the simulation clock to t.
2. Complete all running jobs whose finish tick is t, releasing their resources.
3. Process all job arrivals scheduled for t.
4. Mark impossible jobs as REJECTED.
5. Run the scheduler on eligible queued jobs.
6. Reserve resources and start jobs selected successfully.
7. Record events and update relevant metrics.
8. Check whether the simulation has finished.

This order allows a job completing at tick t to release resources before jobs arriving at tick t are scheduled.

## 9. Execution Duration

When a job starts at tick s with execution duration d:

Finish tick = s + d.

A job with duration 1 that starts at tick 0 completes at tick 1.

- Resources remain allocated throughout execution.
- The job completes when the simulation reaches its finish tick.
- Completion releases all resources reserved for that job.
- A job must not execute for less than or more than its configured duration under normal conditions.

## 10. Simulation Termination

The initial simulation terminates when all jobs have reached terminal states and no future events remain.

Terminal states are COMPLETED, REJECTED, CANCELLED and FAILED.

If jobs remain queued but cannot currently be allocated, the engine must distinguish a genuine deadlock or stalled workload from normal ongoing execution.

The engine must not advance indefinitely without a defined stopping condition. Support a configurable maximum tick limit or another explicit safeguard.

## 11. Workload Reproducibility

- Workloads may be entered manually or generated automatically.
- The workload generator must support a configurable random seed.
- The generated workload must be retained so that it can be reused.
- Strategy comparisons must use the same jobs, arrival ticks, requirements, durations, priorities and initial server configuration.
- Each strategy run must begin with fresh server resources, fresh job states, a reset simulation clock and reset metrics.
- One run must not affect another run's results.

## 12. Performance Metrics

### 12.1 Job Counts

Track:
- Total submitted jobs.
- Completed jobs.
- Rejected jobs.
- Cancelled jobs.
- Failed jobs.
- Jobs still queued or running when a simulation limit is reached.

Counts must be mutually consistent with job states.

### 12.2 Waiting Time

For each job that starts:

Waiting time = Start tick - Arrival tick.

Average waiting time is the arithmetic mean of waiting times for jobs that actually started. Report the sample count and do not silently treat jobs that never started as having zero waiting time.

### 12.3 Throughput

Throughput = Number of completed jobs / Elapsed simulation time.

If elapsed simulation time is zero, throughput must be reported as undefined or not applicable rather than dividing by zero.

### 12.4 Resource Utilization

For each resource type:

Utilization = Total allocated resource-time / Total available resource-time.

For a fixed-capacity server over a measured interval, resource-time is calculated by summing resource usage across ticks and dividing by its capacity multiplied by the number of measured ticks.

Report utilization separately for CPU, RAM, storage and GPU where meaningful. A resource type with zero total capacity must be reported as not applicable.

## 13. Failures and Recovery

Server failures, recovery and job reallocation are optional extensions.

Before implementing them, define:
- When a server becomes unavailable.
- What happens to its running jobs.
- Whether affected jobs fail, restart or return to the queue.
- Whether resources are released immediately.
- How recovery changes server availability.
- How interrupted jobs affect waiting time, throughput and failure metrics.

The core implementation must work correctly without failure simulation.

## 14. Error Handling

- Invalid resource requirements must be rejected before allocation.
- Resource over-allocation must never occur.
- Failed reservations must not modify resource state.
- Duplicate completion events must not release resources twice.
- Invalid state transitions must be reported clearly.
- Invalid configurations and unreadable input files must produce understandable errors.
- Ordinary lack of available capacity is not an exceptional program failure.

## 15. Acceptance Tests

The implementation must demonstrate that:

1. A feasible job starts when a suitable server is available.
2. A future job cannot start before its arrival tick.
3. A job completes at its expected finish tick.
4. Completion releases the exact resources reserved.
5. An impossible job is rejected.
6. A temporarily infeasible job remains queued.
7. No resource dimension exceeds server capacity.
8. First Fit selects the first feasible server.
9. Best Fit selects the server with the lowest defined residual-capacity score.
10. Load Balanced selects the server with the lowest projected average utilization.
11. Simultaneous completion and arrival events follow the documented tick order.
12. Identical workloads and configurations produce reproducible results.
13. Metrics match the recorded simulation events.
14. A simulation limit prevents indefinite execution.
15. All strategies operate through the same allocation interface.
