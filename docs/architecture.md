# Orchestrix — System Architecture

## 1. Project Overview

Orchestrix is a C++17 command-line discrete-event simulator for adaptive data-center resource allocation.

The system models heterogeneous servers, computational jobs, resource constraints and job execution over simulated time. It supports multiple allocation strategies and compares their performance using measurable metrics.

The architecture should remain modular, testable and understandable. Components must have clearly defined responsibilities and interact through explicit interfaces.

## 2. Architectural Principles

- **Encapsulation:** Keep resource capacities, allocation state and job state private, exposing controlled operations.
- **Abstraction:** Define common interfaces for allocation strategies and other genuinely interchangeable behaviors.
- **Inheritance:** Use base classes and derived classes where they represent meaningful server or workload specializations.
- **Polymorphism:** Allow the scheduler to use different allocation strategies through a common abstract interface.
- **Single responsibility:** Keep simulation timing, allocation decisions, resource accounting, CLI interaction and reporting separate.
- **Composition:** Build the simulation engine using collaborating components rather than placing the entire system in one class.
- **Testability:** Keep core logic independent of terminal input and output so it can be tested automatically.

## 3. Core Components

### 3.1 ResourceRequirements

Represents the resources required by a job.

Fields:
- CPU cores
- RAM in GB
- Storage in GB
- GPU devices

Requirements must be non-negative and valid. Resource comparisons must use consistent units.

### 3.2 Job

Represents a computational workload.

Responsibilities:
- Store job ID and workload requirements.
- Store priority, arrival time and execution duration.
- Track lifecycle state and relevant timestamps.
- Track the assigned server, if any.
- Expose controlled state transitions.

Possible workload specializations include CPUJob, GPUJob, StorageJob and HybridJob. Use specialization only where it provides meaningful differences in compatibility or behavior.

### 3.3 Server

Represents a server with finite resource capacity.

Responsibilities:
- Store server ID and total resource capacities.
- Track available and allocated resources.
- Check resource feasibility.
- Reserve resources for a job.
- Release previously reserved resources safely.
- Track whether the server is available for allocation.

Derived server types may include ComputeServer, GPUServer and StorageServer. All servers must obey the same resource-accounting invariants.

### 3.4 DataCenter

Owns or manages the collection of servers.

Responsibilities:
- Register and retrieve servers.
- Provide access to eligible servers for scheduling.
- Expose server state for inspection and metrics.
- Prevent duplicate server IDs.

### 3.5 ResourceManager

Coordinates resource reservation and release.

Responsibilities:
- Verify that a job can run on a server.
- Validate resource requirements and server compatibility.
- Reserve resources atomically.
- Release resources exactly once.
- Prevent over-allocation and negative available capacity.

The ResourceManager is the authoritative coordinator for allocation accounting. A failed reservation must not leave partially allocated resources.

### 3.6 AllocationStrategy

An abstract interface for selecting a suitable server.

Implementations:
- FirstFit
- BestFit
- LoadBalanced

Responsibilities:
- Evaluate candidate servers.
- Select a feasible server according to the chosen policy.
- Return a clear no-candidate result when no server is currently suitable.
- Provide a concise explanation of the decision where practical.

Strategies select servers; the ResourceManager validates and performs reservations.

### 3.7 Scheduler

Coordinates job selection and allocation decisions.

Responsibilities:
- Inspect waiting jobs.
- Ask the configured strategy to select a server.
- Request resource reservation.
- Start eligible jobs after successful allocation.
- Leave temporarily infeasible jobs waiting.
- Update job states consistently.

The Scheduler must not duplicate the ResourceManager's resource-accounting logic.

### 3.8 SimulationEngine

Coordinates the simulation lifecycle.

Responsibilities:
- Maintain the current simulation tick.
- Process job arrivals and completion events.
- Invoke the Scheduler at the defined scheduling point.
- Track running jobs and their finish times.
- Release resources when jobs complete.
- Stop when the configured stopping condition is met.

The engine coordinates other components but does not implement individual allocation algorithms.

### 3.9 WorkloadGenerator

Creates reproducible synthetic workloads.

Responsibilities:
- Generate jobs with valid requirements and arrival times.
- Support configurable workload size and characteristics.
- Accept an optional random seed.
- Produce a workload that can be reused for strategy comparisons.

Manual job entry may be handled separately by the CLI.

### 3.10 MetricsCollector

Tracks simulation performance, including:
- Jobs completed, rejected, cancelled and failed.
- Average waiting time.
- Throughput.
- Resource utilization.
- Optional migration and energy metrics when those features are implemented.

Metrics must be derived from actual simulation events and state, not estimated from hard-coded examples.

### 3.11 FailureManager

An optional extension for simulated server failures and recovery.

Responsibilities:
- Change server availability.
- Identify jobs affected by a failure.
- Coordinate job failure, retry or reallocation according to documented rules.
- Prevent duplicate resource release.

Implement this component after normal scheduling and execution work reliably.

### 3.12 ReportGenerator

Formats simulation results for users.

Responsibilities:
- Display summary metrics.
- Compare allocation strategies.
- Export supported results to CSV or text.
- Keep report formatting separate from simulation logic.

### 3.13 CLI

Provides the user-facing interface.

Responsibilities:
- Configure the data center.
- Create or generate workloads.
- Select an allocation strategy.
- Start and inspect simulations.
- Display server resources, job states and event logs.
- Request comparisons and report exports.

The CLI must call application components rather than implement scheduling or resource management itself.

## 4. Component Interaction

The normal execution flow is:

1. The CLI configures the data center and workload.
2. The SimulationEngine processes events as simulated time advances.
3. Newly arrived jobs become eligible for scheduling.
4. The Scheduler asks the selected AllocationStrategy for a candidate server.
5. The ResourceManager validates and reserves resources.
6. The job enters the running state with a scheduled finish tick.
7. At completion, the engine releases resources and updates the job state.
8. MetricsCollector records the event and updates relevant metrics.
9. ReportGenerator presents the results.

The dependency direction should remain clear: the CLI depends on application components, while domain classes should not depend on terminal input or report formatting.

## 5. Suggested Directory Structure

```text
Orchestrix/
├── AGENTS.md
├── README.md
├── CMakeLists.txt
├── .gitignore
├── docs/
│   ├── requirements.md
│   ├── architecture.md
│   └── simulation-rules.md
├── include/
│   ├── ResourceRequirements.h
│   ├── Job.h
│   ├── Server.h
│   ├── DataCenter.h
│   ├── ResourceManager.h
│   ├── AllocationStrategy.h
│   ├── Scheduler.h
│   ├── SimulationEngine.h
│   ├── WorkloadGenerator.h
│   ├── MetricsCollector.h
│   ├── FailureManager.h
│   └── ReportGenerator.h
├── src/
│   ├── main.cpp
│   └── implementation files matching the headers
└── tests/
    └── automated tests
```

This is a target structure, not a requirement to create every file immediately. Add components as their implementation milestones begin.

## 6. Error Handling

- Reject invalid configurations before simulation begins.
- Reject jobs whose requirements exceed the capacity of every compatible server.
- Treat temporary resource shortages as scheduling outcomes, not exceptional program failures.
- Prevent invalid state transitions.
- Report file-reading and report-writing failures clearly.
- Use exceptions for invalid operations or unrecoverable errors, not ordinary scheduling decisions.

## 7. Testing Requirements

Test each component independently before integrating it.

At minimum, verify:
- Resource feasibility and reservation.
- Resource release and accounting invariants.
- Server and job validation.
- Each allocation strategy.
- Job arrivals and completion timing.
- Reproducibility of workload generation.
- Metric calculations.
- Integration of the complete simulation.

## 8. Scope Boundaries

The initial implementation is a local CLI simulation.

It does not require real cloud infrastructure, networking, distributed execution, a database, a graphical interface or machine learning.

Prioritize a correct, testable simulation engine before optional failure recovery, migration and energy-aware scheduling.
