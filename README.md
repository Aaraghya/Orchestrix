### A Discrete-Event Simulator for Adaptive Data-Center Resource Allocation

_**Orchestrix**_ is a C++ project developed for our Semester 3 Object-Oriented Programming with C++ course.

It simulates job scheduling and resource allocation across heterogeneous data-center servers with varying CPU, RAM, storage and GPU capacities. A discrete simulation clock drives arrivals, queueing, scheduling, execution and completion, then reports performance metrics for three allocation strategies.

## Features

- Domain model: jobs, resource requirements, and Compute, GPU and Storage servers.
- Atomic resource reservation and release with over-allocation protection.
- First Fit, Best Fit and Load Balanced allocation with documented tie-breaking.
- Integer-tick simulation with FIFO queueing, rejection of impossible jobs and a tick-limit safeguard.
- Reproducible workload generation from a fixed seed using `std::mt19937`.
- Metrics: job outcomes (completed, rejected, cancelled, failed, queued, running, and still active), completion/rejection rates, average waiting time, throughput and resource-time utilization.
- Strategy comparison on an identical workload with fresh state for every run, including a compact job outcome summary table.
- Dynamic server resource status display tracking available CPU, RAM, storage, and GPU over time.
- Event log filtering by event type (job arrivals, starts, completions, and rejections).
- Interactive ASCII command-line interface with 8 menu options.

## Tech Stack

- **Language:** C++17
- **Build System:** CMake
- **Compiler:** GCC
- **Testing:** CTest

## Topics

- Discrete-Event Simulation
- Resource Allocation and Scheduling Algorithms
- Performance Analysis and Optimization

## Requirements

- C++17 compiler (GCC via MSYS2 UCRT64 on Windows)
- CMake 3.20 or newer
- Ninja (recommended generator)

## Build, test and run

From the MSYS2 UCRT64 shell:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/orchestrix.exe
```

From Windows PowerShell (toolchain paths explicit):

```powershell
C:\msys64\ucrt64\bin\cmake.exe -S . -B build -G Ninja
C:\msys64\ucrt64\bin\cmake.exe --build build
C:\msys64\ucrt64\bin\ctest.exe --test-dir build --output-on-failure
.\build\orchestrix.exe
```

> **Note on test suites:** Automated milestone tests M5 and M6 each have 1 known pre-existing failing assertion (`test_milestone5`: 42 passed, 1 failed; `test_milestone6`: 17 passed, 1 failed) that predates these features. All other milestone unit tests and CLI smoke tests pass.

## Using the CLI

The interactive menu provides 8 options:

- **Options 1–3:** Run simulation with a single strategy (**1:** First Fit, **2:** Best Fit, **3:** Load Balanced) and report job outcomes, performance metrics, and resource utilization. Individual reports include a breakdown of `Queued` and `Running` counts under `Still active`.
- **Option 4:** Compare all three strategies on an identical generated workload, followed by a compact side-by-side job outcome summary table.
- **Option 5:** Configure workload parameters (job count, server count, and random seed).
- **Option 6:** Exit the simulator.
- **Option 7:** Server status display showing available resources and how they change during a simulation run.
- **Option 8:** Filter simulation event log by event type.

See [docs/demo-scenarios.md](docs/demo-scenarios.md) for reproducible demonstrations.

### Job Outcome Summary (Option 4)

Option 4 evaluates First Fit, Best Fit, and Load Balanced on an identical job set and appends a compact comparison table of final job counts:

```
-------------------------------------------------------
  Job Outcome Summary (Final Job Counts)
-------------------------------------------------------
  Strategy        Completed  Rejected   Queued  Running
  -----------------------------------------------------
  FirstFit               17         3        0        0
  BestFit                17         3        0        0
  LoadBalanced           17         3        0        0
-------------------------------------------------------
```

Completed, rejected, queued, and running counts are tracked separately in `SimulationMetrics` (where `stillActive` equals queued + running + pending). Job outcome counts often tie across strategies because permanent job rejection depends on whether a job's requirements exceed data-center server capacities rather than on the scheduling policy; average waiting time is where strategies differ.

### Server Status Display (Option 7)

Option 7 runs First Fit and displays every server's ID, type, total capacity, and currently available CPU, RAM, storage, and GPU at the initial state and after each job start and resource release, by replaying the engine event log via `ReportGenerator::generateServerStatus`. Note that for large workloads, the full snapshot output is long.

Excerpt of initial state and a job start/release:

```
=======================================================
  Server Resource Status
=======================================================
  (Avl = available; Cap = total capacity; units: cores / GB / devices)

  [tick 0 — initial state]
  Server  Type          Cap.CPU Avl.CPU Cap.RAM Avl.RAM Cap.Stor  Avl.Stor  Cap.GPU Avl.GPU 
  ------------------------------------------------------------------------------------------
  srv-0   ComputeServer 28.0    28.0    66.0    66.0    975.0     975.0     0.0     0.0     
  srv-1   GPUServer     11.0    11.0    63.0    63.0    79.0      79.0      4.0     4.0     
  srv-2   ComputeServer 12.0    12.0    79.0    79.0    658.0     658.0     0.0     0.0     
  srv-3   ComputeServer 12.0    12.0    93.0    93.0    234.0     234.0     0.0     0.0     
  srv-4   ComputeServer 10.0    10.0    27.0    27.0    480.0     480.0     0.0     0.0     

  [tick 1 — job job-17 started on srv-1]
  Server  Type          Cap.CPU Avl.CPU Cap.RAM Avl.RAM Cap.Stor  Avl.Stor  Cap.GPU Avl.GPU 
  ------------------------------------------------------------------------------------------
  srv-0   ComputeServer 28.0    28.0    66.0    66.0    975.0     975.0     0.0     0.0     
  srv-1   GPUServer     11.0    9.0     63.0    52.0    79.0      79.0      4.0     2.0     
  srv-2   ComputeServer 12.0    12.0    79.0    79.0    658.0     658.0     0.0     0.0     
  srv-3   ComputeServer 12.0    12.0    93.0    93.0    234.0     234.0     0.0     0.0     
  srv-4   ComputeServer 10.0    10.0    27.0    27.0    480.0     480.0     0.0     0.0     

  [tick 4 — job job-17 released resources from srv-1]
  Server  Type          Cap.CPU Avl.CPU Cap.RAM Avl.RAM Cap.Stor  Avl.Stor  Cap.GPU Avl.GPU 
  ------------------------------------------------------------------------------------------
  srv-0   ComputeServer 28.0    20.0    66.0    53.0    975.0     510.0     0.0     0.0     
  srv-1   GPUServer     11.0    11.0    63.0    63.0    79.0      79.0      4.0     4.0     
  srv-2   ComputeServer 12.0    12.0    79.0    79.0    658.0     658.0     0.0     0.0     
  srv-3   ComputeServer 12.0    12.0    93.0    93.0    234.0     234.0     0.0     0.0     
  srv-4   ComputeServer 10.0    10.0    27.0    27.0    480.0     480.0     0.0     0.0     
```

### Event Log Filtering (Option 8)

Option 8 runs a simulation (First Fit) and lets the user choose an event type (arrival, start, completion, or rejection) to inspect matching records using `ReportGenerator::generateFilteredEvents`.

Sample output when filtering by rejections:

```
------------------------------------------------------------
  Simulation Events: JOB_REJECTED
------------------------------------------------------------
  Tick    Job ID      Server ID   Detail
  ----------------------------------------------------------
  7       job-14      -           No compatible server has sufficient total capacity
  14      job-16      -           No compatible server has sufficient total capacity
  50      job-12      -           No compatible server has sufficient total capacity
------------------------------------------------------------
  Total matching events: 3
------------------------------------------------------------
```

## Project Status

Completed!

## Documentation

- [docs/architecture.md](docs/architecture.md) — component design and responsibilities
- [docs/demo-scenarios.md](docs/demo-scenarios.md) — reproducible CLI demonstrations

---

*Built with C++ as part of our Semester 3 OOPs coursework.*
