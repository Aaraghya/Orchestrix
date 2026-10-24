# Orchestrix — Requirements

## Objective
Build a C++17 CLI-based discrete-event simulator that models a small data center and compares resource-allocation strategies under identical workloads.

## Core requirements
1. Model heterogeneous servers with CPU, RAM, storage and GPU resources.
2. Model jobs with resource requirements, priorities, arrival times, execution durations and lifecycle states.
3. Prevent resource over-allocation and invalid resource release.
4. Implement First Fit, Best Fit and Load Balanced allocation strategies.
5. Advance simulation time using discrete ticks.
6. Process job arrivals, queueing, execution and completion.
7. Release resources correctly when jobs finish.
8. Display job states, server resources and simulation events through a CLI.
9. Calculate resource utilization, throughput, average waiting time and rejected-job counts.
10. Compare strategies using the same initial server configuration and workload.
11. Provide tests for normal behavior and edge cases.

## Constraints
- C++17.
- CMake build system.
- Standard library preferred.
- No GUI or external cloud infrastructure.
- Implement the core requirements before optional extensions.

## Acceptance criteria
- The project builds from a clean checkout.
- Resources cannot be over-allocated.
- Completed jobs release their resources.
- Future jobs do not execute before their arrival.
- Each allocation strategy works through a common interface.
- Strategy comparisons use reproducible inputs.
- Metrics reflect the actual simulation results.
