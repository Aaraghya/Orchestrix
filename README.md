### A Discrete-Event Simulator for Adaptive Data-Center Resource Allocation

_**Orchestrix**_ is a C++ project developed for our Semester 3 Object-Oriented Programming with C++ course.

It simulates job scheduling and resource allocation across heterogeneous data-center servers with varying CPU, RAM, storage and GPU capacities. A discrete simulation clock drives arrivals, queueing, scheduling, execution and completion, then reports performance metrics for three allocation strategies.

## Features

- Domain model: jobs, resource requirements, and Compute, GPU and Storage servers.
- Atomic resource reservation and release with over-allocation protection.
- First Fit, Best Fit and Load Balanced allocation with documented tie-breaking.
- Integer-tick simulation with FIFO queueing, rejection of impossible jobs and a tick-limit safeguard.
- Reproducible workload generation from a fixed seed using `std::mt19937`.
- Metrics: job outcomes, completion/rejection rates, average waiting time, throughput and resource-time utilization.
- Strategy comparison on an identical workload with fresh state for every run.
- Interactive ASCII command-line interface.

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

## Using the CLI

Menu options 1–3 run one strategy on the generated workload, option 4 compares all three strategies, option 5 configures the job count, server count and random seed, and option 6 exits. See [docs/demo-scenarios.md](docs/demo-scenarios.md) for three reproducible demonstrations.

## Project Status

Milestones 1–6 are complete: the project builds cleanly, 212 unit tests and 4 CLI smoke tests pass, and the simulation behavior matches [docs/simulation-rules.md](docs/simulation-rules.md).

Optional extensions not implemented: server failure/recovery, priority scheduling, energy-aware allocation and CSV export.

## Documentation

- [docs/requirements.md](docs/requirements.md) — project requirements and acceptance criteria
- [docs/architecture.md](docs/architecture.md) — component design and responsibilities
- [docs/simulation-rules.md](docs/simulation-rules.md) — timing, lifecycle, scheduling and metric rules
- [docs/demo-scenarios.md](docs/demo-scenarios.md) — reproducible CLI demonstrations

---

*Built with C++ as part of our Semester 3 OOPs coursework.*
