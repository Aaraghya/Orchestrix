<div align="center">

# Orchestrix

### A Discrete-Event Simulator for Adaptive Data-Center Resource Allocation

`C++17`   `CMake`   `Ninja`   `CTest`   `Standard library only`   `Interactive CLI`

[Screenshots](#screenshots) / [Overview](#overview) / [Features](#features) / [Strategies](#allocation-strategies) / [How it works](#how-it-works) / [Getting started](#prerequisites-and-installation) / [Running](#running-orchestrix)

</div>

***Orchestrix*** is a C++17 command-line project developed for our Semester 3 Object-Oriented Programming with C++ course (CSE 202, end-semester project). It simulates job scheduling and resource allocation across heterogeneous data-center servers with different CPU, RAM, storage and GPU capacities. An integer-tick simulation clock drives arrivals, queueing, scheduling, execution and completion, then reports performance metrics for three allocation strategies.

| | |
|---|---|
| [![Orchestrix main menu with the logo, workload settings and eight numbered options](screenshots/menu.png)](screenshots/menu.png) | [![Results of a First Fit run with 18 allocated jobs in green and 2 rejected jobs in red](screenshots/single-run.png)](screenshots/single-run.png) |
| *The main menu* | *One strategy run, paginated, with job statuses colour-coded* |

---

## Screenshots

### Strategy comparison

Every strategy runs on the same generated workload with fresh state, then the report compares them.

| | |
|---|---|
| [![Strategy comparison report header with the First Fit block: outcomes, wait time and utilization](screenshots/comparison-top.png)](screenshots/comparison-top.png) | [![Load Balanced report block followed by the job outcome summary table for all three strategies](screenshots/comparison.png)](screenshots/comparison.png) |
| *Report header and the First Fit block* | *The last block and the final job outcome summary* |

### Configuring the workload

Choose the job count, server count and seed. The same seed always reproduces the same workload.

[![Workload configuration prompts for job count, server count and seed](screenshots/configure.png)](screenshots/configure.png)

### Server status over time

Shows how available CPU, RAM, storage and GPU change on each server as jobs start and finish.

[![Server resource status tables for each tick, with GPU, Compute and Storage servers in different colours](screenshots/server-status.png)](screenshots/server-status.png)

### Event log filtering

Filter the simulation event log by event type, here showing only rejected jobs.

[![Event log filtered to rejected jobs, listing the tick, job and reason](screenshots/event-log.png)](screenshots/event-log.png)

### Automated tests

[![CTest output showing all Orchestrix tests passing](screenshots/tests.png)](screenshots/tests.png)

---

## Overview

Orchestrix models a small data center as a set of servers that own finite resources. Jobs arrive at known ticks, declare their resource requirements and duration, wait in a FIFO queue, and are placed on a compatible server by one of three allocation strategies. When a job finishes, its resources are released and become available to waiting jobs.

The simulator is deterministic for a given workload configuration and seed. The same job count, server count and seed always produce the same workload, so strategies can be compared fairly on identical input.

The project is intentionally a single-threaded, standard-library-only C++ application. It has no GUI, database, network layer or external runtime dependencies.

## Features

- **Domain model:** jobs, resource requirements, and Compute, GPU and Storage servers.
- **Safe resource accounting:** atomic reservation and release, with over-allocation and over-release protection.
- **Three allocation strategies:** First Fit, Best Fit and Load Balanced, with deterministic tie-breaking.
- **Discrete-event engine:** integer ticks, FIFO queueing, permanent rejection of impossible jobs and a tick-limit safeguard.
- **Reproducible workloads:** generated from `std::mt19937`, with separate RNG streams for servers and jobs.
- **Paginated results:** 20 allocation records per page, with `N` / `P` / `Q` navigation.
- **Fair strategy comparison:** the same workload and fresh state for every run, plus a compact job-outcome summary table.
- **Metrics:** job outcomes, completion and rejection rates, average waiting time, throughput and resource-time utilization.
- **Server status timeline:** reconstructs how available CPU, RAM, storage and GPU change during a run.
- **Event-log filtering:** job arrivals, starts, completions and rejections.
- **Colour-coded interactive CLI:** 8 menu options, input validation, and colour that switches off for piped output.

## Allocation Strategies

All strategies receive the same `DataCenter` and are asked to place a `Job`. A strategy only selects a server; the `Scheduler` performs the atomic resource reservation after selection.

| Strategy | Selection rule | Tie-breaking |
|---|---|---|
| **First Fit** | Iterate servers in data-center insertion order and choose the first server that is available, type-compatible and has enough available resources. | Insertion order — the first match wins. |
| **Best Fit** | For every compatible server, compute a score as the sum of `(available − required) / capacity` over each resource dimension the job requests. Choose the smallest score, i.e. the server that would be left with the least proportional free space. | Lexicographically smaller server ID. |
| **Load Balanced** | For every compatible server, compute the average projected utilization `(allocated + required) / capacity` over all dimensions with positive capacity. Choose the lowest projected utilization. | Lexicographically smaller server ID. |

A server is compatible only when it is available and its type accepts the job:

- `ComputeServer` accepts any workload that fits its capacity.
- `GPUServer` accepts only jobs with `gpu > 0`.
- `StorageServer` accepts only jobs with `storage > 0`.

A job that cannot fit on any compatible server at its arrival tick is rejected only if no server could ever hold it based on total capacity and type. Temporarily full servers do not cause permanent rejection; the job waits and is retried on later ticks.

## How it works

### Components

| Layer | Classes | Responsibility |
|---|---|---|
| Model | `ResourceRequirements`, `Job`, `Server`, `DataCenter` | Describe jobs, the four resource dimensions and the pool of servers |
| Servers | `ComputeServer`, `GPUServer`, `StorageServer` | Share one accounting base class and add their own compatibility rule |
| Strategies | `AllocationStrategy`, `FirstFit`, `BestFit`, `LoadBalanced` | Choose a server for a job; they never reserve resources themselves |
| Scheduling | `Scheduler` | Asks the strategy for a server, then reserves resources atomically |
| Simulation | `SimulationEngine`, `SimulationEvent`, `WorkloadGenerator` | Advance the tick clock, log events, generate reproducible workloads |
| Analysis | `MetricsCollector`, `SimulationMetrics`, `StrategyComparison` | Derive metrics from a finished run and compare strategies on identical input |
| Presentation | `ReportGenerator`, `Terminal.h`, `src/main.cpp` | Format every report, add colour, and run the interactive menu |

### Object-oriented design

| Concept | Where | How it is used |
|---|---|---|
| Encapsulation | `Server` | Owns its capacity and allocation. "Available" is computed as total minus allocated, and a failed reserve or release throws without changing anything. |
| Inheritance | `Server` → `ComputeServer`, `GPUServer`, `StorageServer` | Derived servers inherit all accounting and only add which jobs they accept. |
| Polymorphism | `AllocationStrategy` → `FirstFit`, `BestFit`, `LoadBalanced` | `Scheduler` calls `select()` through the base interface and never knows which strategy it holds. |
| Abstraction | `AllocationStrategy`, `Server::canRunJob()` | Pure virtual functions hide the selection and compatibility rules behind a small interface. |
| Separation of concerns | `MetricsCollector`, `ReportGenerator` | Stateless classes that derive or format results without touching simulation state. |

### Execution flow

```mermaid
flowchart TD
    CLI["CLI — src/main.cpp"]
    WG["WorkloadGenerator"]
    SPECS["ServerSpec / JobSpec"]
    SC["StrategyComparison"]
    ENG["SimulationEngine"]
    SCH["Scheduler"]
    STR["AllocationStrategy<br/>First Fit / Best Fit / Load Balanced"]
    DC["DataCenter"]
    SRV["Server hierarchy<br/>ComputeServer / GPUServer / StorageServer"]
    JOB["Job"]
    RR["ResourceRequirements"]
    MC["MetricsCollector"]
    SM["SimulationMetrics"]
    RG["ReportGenerator"]

    CLI --> WG
    WG --> SPECS
    SPECS --> CLI
    CLI --> DC
    CLI --> JOB
    DC --> SRV
    SRV --> RR
    JOB --> RR
    CLI --> ENG
    CLI --> SC
    SC --> ENG
    SC --> MC
    SC --> RG
    ENG --> SCH
    SCH --> STR
    STR --> DC
    SCH --> DC
    ENG --> DC
    ENG --> JOB
    ENG --> MC
    MC --> SM
    SM --> RG
    CLI --> RG
```

The engine advances one tick at a time and performs a fixed sequence:

1. Complete running jobs whose finish tick equals the current tick: release resources, log `RESOURCE_RELEASED`, then complete the job and log `JOB_COMPLETED`.
2. Process arrivals at this tick: move each job from `PENDING` to `QUEUED`, log `JOB_ARRIVED`, reject it if it can never fit, otherwise append it to the FIFO wait queue.
3. Scan the entire wait queue in FIFO order and try to schedule every job. Jobs that cannot be placed remain queued and are retried next tick; a blocked job does not prevent later jobs from being attempted.
4. Stop when all jobs are terminal, when no future events remain, or when the tick limit is reached.

`MetricsCollector` is stateless and reads the finished engine plus the data center. It reconstructs allocation intervals from `JOB_STARTED` and `RESOURCE_RELEASED` events. `ReportGenerator` is also stateless and formats all console output: paginated results, comparison reports, filtered event logs and server-status timelines.

### Project structure

```
Orchestrix/
├── CMakeLists.txt          # build and test configuration
├── README.md
├── screenshots/            # images used in this README
├── docs/
│   ├── architecture.md     # components and execution flow
│   └── demo-scenarios.md   # reproducible demo walkthroughs
├── include/                # headers: model, strategies, engine, reports, Terminal.h
├── src/                    # implementations and main.cpp (the CLI)
└── tests/                  # milestone tests, pagination, terminal and CLI smoke tests
```

## Technology Stack

- **Language:** C++17 (no compiler-specific extensions)
- **Build system:** CMake 3.20 or newer
- **Generator:** Ninja (recommended) or any CMake generator
- **Testing:** CTest with self-contained C++ test executables
- **Libraries:** C++ standard library only
- **Warnings:** `/W4` on MSVC, `-Wall -Wextra -Wpedantic` elsewhere

The verified development toolchain is GCC 16.1.0 (MSYS2 UCRT64), CMake 4.3.3 and Ninja 1.13.2 on Windows.

## Prerequisites and Installation

You need:

- A C++17 compiler (GCC, Clang or MSVC).
- CMake 3.20 or newer.
- Ninja is recommended but optional.
- Git to clone the repository.

On Windows, the verified setup uses the MSYS2 UCRT64 environment. Install the toolchain from an MSYS2 UCRT64 shell:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc \
                   mingw-w64-ucrt-x86_64-cmake \
                   mingw-w64-ucrt-x86_64-ninja
```

Then clone the repository:

```bash
git clone https://github.com/Aaraghya/Orchestrix.git
cd Orchestrix
```

On Linux or macOS, install a C++17 compiler and CMake through your package manager; no project-specific dependencies are required.

## Build Instructions

From the repository root in the MSYS2 UCRT64 shell:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

The executable is produced at `build/orchestrix.exe` on Windows and `build/orchestrix` on POSIX systems. Omit `-G Ninja` to use the platform default generator.

From Windows PowerShell with explicit toolchain paths:

```powershell
C:\msys64\ucrt64\bin\cmake.exe -S . -B build -G Ninja
C:\msys64\ucrt64\bin\cmake.exe --build build
C:\msys64\ucrt64\bin\ctest.exe --test-dir build --output-on-failure
.\build\orchestrix.exe
```

To rebuild from scratch, remove the `build/` directory and run the configure step again.

## Running Orchestrix

Start the interactive simulator:

```bash
./build/orchestrix.exe      # Windows
./build/orchestrix          # Linux / macOS
```

### Colour output

When Orchestrix runs in a terminal, the output is colour-coded: headings in cyan, `Allocated` / `Completed` in green, `Rejected` in red, `Queued` in yellow, utilization percentages from green to red, and the server types in their own colours. Colour is only added for interactive terminals, so piped output and the automated tests stay plain text.

| Setting | Effect |
|---|---|
| `NO_COLOR=1` | Turn colour off |
| `ORCHESTRIX_COLOR=always` | Force colour on (for example when the terminal is not detected) |
| `ORCHESTRIX_COLOR=never` | Turn colour off |

The colour logic lives in `include/Terminal.h` and is covered by `tests/test_terminal.cpp`.

## Contributing

1. Fork the repository and create a focused branch, for example `feature/my-change`.
2. Add or update tests for meaningful behavior and edge cases.
3. Build and run the full test suite locally.
4. Open a pull request describing the change, the motivation, and the tests run.

Please do not silently change agreed interfaces or simulation rules, and do not mix unrelated refactors into a single change. Keep changes focused and reviewable.

Acknowledgements:

- Developed as part of the Semester 3 **Object-Oriented Programming with C++** course.
- Built with the C++ standard library, CMake, CTest and Ninja.
- Reproducible demonstrations are documented in [docs/demo-scenarios.md](docs/demo-scenarios.md).
