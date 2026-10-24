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

## Project Status

Completed!

## Documentation

- [docs/architecture.md](docs/architecture.md) — component design and responsibilities
- [docs/demo-scenarios.md](docs/demo-scenarios.md) — reproducible CLI demonstrations

---

*Built with C++ as part of our Semester 3 OOPs coursework.*
