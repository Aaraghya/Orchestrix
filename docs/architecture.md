# Orchestrix — System Architecture

## 1. Project Overview

Orchestrix is a C++17 command-line application that simulates data-center job scheduling and resource allocation using discrete-event simulation.

The system models heterogeneous servers with CPU, RAM, storage, and GPU capacities. Jobs progress through arrival, queueing, execution, and completion, while the scheduler allocates resources according to one of three strategies: First Fit, Best Fit, or Load Balanced.

The simulator also generates reproducible workloads, records simulation events, calculates performance metrics, and compares scheduling strategies under equivalent workload conditions.

## 2. Architectural Overview

Orchestrix separates resource and job modeling, server management, scheduling decisions, simulation execution, workload generation, and performance analysis into distinct components.

Its architecture uses object-oriented programming concepts, including encapsulation, inheritance, abstraction, and polymorphism. Server types share a common base class, while allocation strategies implement a shared interface that allows the scheduler to operate with different scheduling policies.

The simulation engine coordinates the job lifecycle and simulation clock. Resource accounting is handled by server operations, and the command-line interface provides access to simulation configuration and results.

## 3. Core Components

### 3.1 ResourceRequirements

`ResourceRequirements` represents the resources requested by a job.

It captures four resource dimensions:

- CPU cores
- RAM in GB
- Storage in GB
- GPU devices

These requirements are used to determine whether a server has sufficient available capacity to accommodate a job.

### 3.2 Job

`Job` represents an individual computational workload in the simulation.

A job stores its identifying information, resource requirements, arrival and execution information, and lifecycle state. Its state reflects its progress through the simulation, including waiting, execution, completion, or rejection.

Job information is used by the scheduler and simulation engine to determine eligibility for execution, track progress, and calculate performance metrics.

### 3.3 Server

`Server` is the common base class for server models. It maintains server resource capacities and allocation state, and provides operations for checking feasibility, reserving resources, and releasing resources.

The implemented server types are:

- **ComputeServer:** Represents compute-oriented server capacity.
- **GPUServer:** Represents server capacity intended for GPU workloads.
- **StorageServer:** Represents storage-oriented server capacity.

Resource reservations and releases are validated by the server operations. This keeps resource accounting associated with the server whose capacity is being modified and prevents resource over-allocation through those operations.

### 3.4 DataCenter

`DataCenter` manages the collection of servers available to the simulation.

It provides the server information used during scheduling and resource allocation. The scheduler and allocation strategies use this collection to identify servers that can accommodate waiting jobs.

### 3.5 AllocationStrategy

`AllocationStrategy` defines the common interface used by the scheduling policies. The implemented strategies are First Fit, Best Fit, and Load Balanced.

**First Fit** selects the first suitable server encountered in the candidate order.

**Best Fit** selects a suitable server according to the strategy's resource-fit criteria.

**Load Balanced** selects a suitable server according to its load-balancing criteria.

Each strategy evaluates server suitability and returns a candidate when one is available. The strategies differ in server selection; resource reservation is performed separately through the server's resource-management operations.

### 3.6 Scheduler

The `Scheduler` coordinates job scheduling and resource allocation.

It examines waiting jobs, uses the configured allocation strategy to select a suitable server, and requests resource reservation. Following a successful allocation, the job can enter execution. Jobs that cannot currently be allocated remain waiting unless their requirements make them impossible to accommodate, in which case they are rejected according to the simulation rules.

The scheduler coordinates these decisions without duplicating the server's resource-accounting implementation.

### 3.7 SimulationEngine

`SimulationEngine` coordinates the discrete-event simulation.

It maintains simulated time and processes job arrivals, scheduling activity, execution, and completion. Running jobs are tracked against their completion times. When a job completes, the engine updates its state and releases its reserved resources.

The engine also handles the configured simulation stopping condition. Its event processing and lifecycle management provide the basis for the metrics calculated after a run.

### 3.8 WorkloadGenerator

`WorkloadGenerator` creates synthetic workloads for simulation runs.

It supports configurable job and server counts and uses a specified random seed to make workload generation reproducible. The same generated workload can be supplied to different scheduling strategies for comparison.

The current command-line interface uses generated workloads; manual job entry is not implemented.

### 3.9 MetricsCollector

`MetricsCollector` calculates performance metrics from the completed simulation's job states and event log.

The reported metrics include:

- Completed and rejected job counts and rates
- Average waiting time
- Throughput
- CPU utilization
- RAM utilization
- Storage utilization
- GPU utilization, where applicable

The collector is stateless: it derives metrics from the results of a completed run rather than maintaining a separate simulation state. Resource utilization is based on resource-time accounting, while job-related metrics are derived from the recorded simulation outcomes.

### 3.10 Command-Line Interface and Reporting

The command-line interface provides the user-facing entry point to Orchestrix. It supports running individual scheduling strategies, comparing strategies, configuring workloads, and viewing simulation results.

The interface displays summary metrics and comparison results in text form. Report presentation remains separate from the core scheduling and simulation logic.

CSV export is not implemented in the current build.

## 4. Component Interaction and Execution Flow

A typical Orchestrix run follows this sequence:

1. The CLI configures the workload and data-center parameters.
2. The workload generator creates the jobs using the configured settings and random seed.
3. The simulation engine advances simulated time and processes job arrivals.
4. Eligible jobs enter the scheduling process.
5. The scheduler asks the selected allocation strategy to identify a suitable server.
6. The selected server validates and reserves the required resources.
7. The allocated job enters execution and is tracked until its completion time.
8. When execution completes, the simulation engine updates the job state and releases the reserved resources.
9. The simulation continues until its stopping condition is reached.
10. The metrics collector derives performance measurements from the completed run, and the CLI presents the results.

During strategy comparison, each strategy runs with a fresh `DataCenter`, job vector, `Scheduler`, and `SimulationEngine`. This prevents mutable state from one run from affecting another and allows the strategies to be evaluated against equivalent workload specifications.

## 5. Resource Allocation and Accounting

Resource allocation is implemented through the interaction between the scheduler, allocation strategies, and server operations.

The allocation strategy determines which suitable server to select. The scheduler requests the reservation, and the server validates the operation against its available capacity. Resource release occurs when the simulation engine processes job completion.

There is no separate `ResourceManager` class in the current implementation. Reservation and release logic resides in the server operations, including `Server::reserveResources()` and `Server::releaseResources()`.

This arrangement keeps the allocation policy separate from the operations that modify resource availability.

## 6. Simulation and Performance Measurement

Orchestrix uses integer simulation ticks to represent the progression of time. Job arrivals, waiting, execution, and completion are processed according to the documented simulation rules.

The workload generator and random seed support reproducible experiments. Strategy comparisons use equivalent workload specifications and independent mutable simulation state.

Performance measurements are calculated from simulation outcomes rather than fixed example values. The reported metrics allow the scheduling strategies to be compared in terms of job completion, rejection, waiting time, throughput, and resource utilization.

The simulator models these operations locally; it does not allocate resources on real data-center infrastructure.

## 7. Error Handling and Validation

Validation is applied to resource requirements, resource reservations, and other operations that could produce invalid simulation state.

The simulation distinguishes between jobs that cannot be accommodated by the available server capacities and jobs that are temporarily waiting for resources. Resource reservation and release operations validate their accounting changes to prevent over-allocation and invalid resource state.

The CLI also handles termination through normal menu interaction and end-of-input conditions, avoiding an unintended infinite menu loop when standard input reaches EOF.

## 8. Testing and Verification

Orchestrix includes automated tests covering individual components and integrated simulation behavior.

The test suite covers resource requirements and accounting, server and job behavior, allocation strategies, scheduling, simulation timing, workload reproducibility, metric calculations, and CLI execution paths.

The latest recorded project validation reported:

- **212 unit tests** across Milestones 1–6
- **4 CLI smoke tests**
- **10/10 CTest entries passing**
- A clean build with zero compiler warnings

These figures describe the recorded validation results and may change as the project evolves.

## 9. Scope and Limitations

Orchestrix is a local C++17 command-line simulation project. Its current scope includes heterogeneous server modeling, three allocation strategies, reproducible workload generation, discrete-event execution, performance measurement, and strategy comparison.

The following features are not implemented in the current build:

- A separate `ResourceManager` component
- Server failure and recovery simulation
- Job migration and reallocation following failures
- CSV report export
- Manual job entry through the CLI
- Energy-aware scheduling
- Real cloud infrastructure integration or distributed execution
- A graphical user interface

These limitations distinguish the implemented simulator from possible future extensions.

## 10. Implementation Summary

Orchestrix uses an object-oriented, component-based architecture to simulate data-center resource allocation and compare scheduling strategies. Its main components are the job and resource models, server hierarchy, data center, allocation strategies, scheduler, simulation engine, workload generator, metrics collector, and command-line interface.

Resource accounting is implemented within server operations, while the simulation engine manages event progression and job completion. Reproducible workload generation and independent simulation runs support meaningful strategy comparisons.

The implementation combines core C++ object-oriented concepts with discrete-event simulation, resource scheduling, automated testing, and performance analysis.
