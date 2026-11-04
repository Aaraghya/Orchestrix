<div align="center">

# Orchestrix

### A Discrete-Event Simulator for Adaptive Data-Center Resource Allocation

`C++17`   `CMake`   `Ninja`   `CTest`   `Standard library only`   `Interactive CLI`

[Screenshots](#screenshots) / [Overview](#overview) / [Features](#features) / [Strategies](#allocation-strategies) / [How it works](#how-it-works) / [Getting started](#getting-started) / [Menu options](#menu-options) / [Team](#team)

</div>

***Orchestrix*** is a C++17 command-line project developed for our Semester 3 Object-Oriented Programming with C++ course (CSE 202, end-semester project). It simulates job scheduling and resource allocation across heterogeneous data-center servers with different CPU, RAM, storage and GPU capacities. An integer-tick simulation clock drives arrivals, queueing, scheduling, execution and completion, then reports performance metrics for three allocation strategies.

| | |
|---|---|
| <a href="screenshots/menu.png"><img src="screenshots/menu.png" height="480" alt="Orchestrix main menu with the logo, workload settings and eight numbered options"></a> | <a href="screenshots/single-run.png"><img src="screenshots/single-run.png" height="480" alt="Results of a First Fit run with 18 allocated jobs in green and 2 rejected jobs in red"></a> |
| *The main menu* | *One strategy run, paginated, with job statuses colour-coded* |

---

## Screenshots

### Strategy comparison

Every strategy runs on the same generated workload with fresh state, then the report compares them.

| | |
|---|---|
| <a href="screenshots/comparison-top.png"><img src="screenshots/comparison-top.png" height="420" alt="Strategy comparison report header with the First Fit block: outcomes, wait time and utilization"></a> | <a href="screenshots/comparison.png"><img src="screenshots/comparison.png" height="420" alt="Load Balanced report block followed by the job outcome summary table for all three strategies"></a> |
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
| **First Fit** | Walk the servers in insertion order and take the first one that is available, type-compatible and has enough free resources. | Insertion order: the first match wins. |
| **Best Fit** | Score each compatible server as the sum of `(available − required) / capacity` over the resources the job requests. The lowest score wins, meaning the server left with the least proportional free space. | Lexicographically smaller server ID. |
| **Load Balanced** | Compute each compatible server's average projected utilization `(allocated + required) / capacity` over every dimension with positive capacity. The lowest wins. | Lexicographically smaller server ID. |

A server is compatible only when it is available and its type accepts the job:

| Server type | Accepts |
|---|---|
| `ComputeServer` | Any workload that fits its capacity |
| `GPUServer` | Only jobs with `gpu > 0` |
| `StorageServer` | Only jobs with `storage > 0` |

A job that cannot fit on any compatible server at its arrival tick is rejected only if no server could ever hold it, judged by total capacity and type. A temporarily full server does not cause a permanent rejection: the job waits and is retried on later ticks.

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

| | |
|---|---|
| **Language** | C++17, no compiler-specific extensions |
| **Build system** | CMake 3.20 or newer, with Ninja recommended |
| **Testing** | CTest with self-contained C++ test executables |
| **Libraries** | C++ standard library only |
| **Warnings** | `/W4` on MSVC, `-Wall -Wextra -Wpedantic` elsewhere |
| **Verified with** | GCC 13.3 on Ubuntu, and GCC 15.2 and 16.1 with CMake 4.x on Windows (MSYS2 UCRT64) |

## Getting started

### Prerequisites

- A C++17 compiler (GCC, Clang or MSVC)
- CMake 3.20 or newer
- Ninja (recommended, optional)
- Git

### Install and run

**Windows (MSYS2 UCRT64)**

1. Install [MSYS2](https://www.msys2.org) and open the **MSYS2 UCRT64** shell from the Start menu.
2. Install the toolchain:
   ```bash
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc \
                      mingw-w64-ucrt-x86_64-cmake \
                      mingw-w64-ucrt-x86_64-ninja git
   ```
3. Clone the repository:
   ```bash
   git clone https://github.com/Aaraghya/Orchestrix.git
   cd Orchestrix
   ```
4. Build:
   ```bash
   cmake -S . -B build -G Ninja
   cmake --build build
   ```
5. Run:
   ```bash
   ./build/orchestrix.exe
   ```

**Linux and macOS**

Install a C++17 compiler, CMake, Ninja and Git with your package manager (for example `sudo apt install g++ cmake ninja-build git`), then:

```bash
git clone https://github.com/Aaraghya/Orchestrix.git
cd Orchestrix
cmake -S . -B build -G Ninja
cmake --build build
./build/orchestrix
```

Omit `-G Ninja` to use your platform's default generator. To rebuild from scratch, delete the `build/` directory and run the configure step again.

<details>
<summary>Windows PowerShell with explicit toolchain paths</summary>

```powershell
C:\msys64\ucrt64\bin\cmake.exe -S . -B build -G Ninja
C:\msys64\ucrt64\bin\cmake.exe --build build
C:\msys64\ucrt64\bin\ctest.exe --test-dir build --output-on-failure
.\build\orchestrix.exe
```

</details>

### Menu options

| Option | What it does |
|---|---|
| `1` | Run the simulation with **First Fit** and show paginated results |
| `2` | Run the simulation with **Best Fit** |
| `3` | Run the simulation with **Load Balanced** |
| `4` | Compare all three strategies on the same generated workload |
| `5` | Configure the workload: job count (1 to 1000), server count (1 to 100) and seed |
| `6` | Show the server status timeline for a First Fit run |
| `7` | Filter the event log by arrivals, starts, completions or rejections |
| `8` | Exit |

On the paginated screen, `N` shows the next page, `P` the previous page and `Q` returns to the menu. The default workload is 20 jobs, 5 servers and seed 42.

### Troubleshooting

| Problem | Fix |
|---|---|
| On Windows, `ctest` reports `Process not started` | The new test programs were probably blocked while being scanned. Run `ctest --test-dir build --rerun-failed --output-on-failure`. |
| No colour appears | Run `ORCHESTRIX_COLOR=always ./build/orchestrix.exe`. |
| `cmd` or PowerShell cannot start `orchestrix.exe` because a DLL is missing | Start it from the UCRT64 shell, or add `C:\msys64\ucrt64\bin` to `PATH` in that window. |
| `pacman` reports download timeouts | Run the command again; it falls back to other mirrors. |

## Team

Built for CSE 202 (Object-Oriented Programming with C++), Semester 3 end-semester project.

| Contributor | GitHub | Contributions |
|---|---|---|
| Aaraghya | [@Aaraghya](https://github.com/Aaraghya) | Project foundation and the core simulator: domain model, allocation strategies and scheduler, simulation engine, metrics collection, the CLI, and the architecture and demo documentation |
| Gayatri | [@gay3-dumb](https://github.com/gay3-dumb) | Event-log filtering, the job outcome summary table, the server status display, README updates for those menu options, and the colour-coded terminal output with its tests |
| Ridhima | [@ridhimaprasad5-gif](https://github.com/ridhimaprasad5-gif) | Contextual explanations in the main menu and the paginated results display |

## Contributing

1. Create a focused branch, for example `feature/my-change`. Collaborators can push it directly; everyone else can fork the repository first.
2. Add or update tests for meaningful behavior and edge cases.
3. Build and run the full test suite locally.
4. Open a pull request describing the change, the motivation, and the tests run.

Please do not silently change agreed interfaces or simulation rules, and do not mix unrelated refactors into a single change. Keep changes focused and reviewable.

Acknowledgements:

- Developed as part of the Semester 3 **Object-Oriented Programming with C++** course.
- Built with the C++ standard library, CMake, CTest and Ninja.
- Reproducible demonstrations are documented in [docs/demo-scenarios.md](docs/demo-scenarios.md).

