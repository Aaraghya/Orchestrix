# Orchestrix — Demo Scenarios

Three reproducible demonstrations for the Semester 3 project review. All outcomes below were produced by the current build and are deterministic for the stated settings.

Start the program from the repository root:

```bash
./build/orchestrix.exe
```

Menu reminder:

```
1. Run simulation - First Fit
2. Run simulation - Best Fit
3. Run simulation - Load Balanced
4. Compare all three strategies
5. Configure workload (job count / seed)
6. Exit
```

---

## Demo 1 — Individual simulation

**Goal:** run one strategy and inspect its metrics.

**Steps**

1. Choose `1` (First Fit). Keep the default settings: 20 jobs, 5 servers, seed 42.
2. Choose `6` to exit.

**What to show**

The default workload is small and has little contention, so most jobs start immediately.

```
Strategy  : FirstFit
Status    : Completed normally
Elapsed   : 54 tick(s)

Job Outcomes
  Submitted   : 20
  Completed   : 18  (90.0%)
  Rejected    : 2  (10.0%)
  Still active: 0

Performance
  Avg wait time  : 0.000 tick(s)  [sample n=18]
  Throughput     : 0.333 jobs/tick

Resource Utilization (resource-time accounting)
  CPU     : 9.9%
  RAM     : 8.2%
  Storage : 3.2%
  GPU     : 7.2%
```

**Talking points**

- Two jobs are rejected at arrival because their requirements exceed the total capacity of every compatible server.
- `Elapsed = 54` is the tick at which the last job completed.
- `Throughput = 18 completed / 54 ticks = 0.333 jobs/tick`.
- Utilization is resource-time: allocated resource-ticks divided by capacity-ticks over the whole run. It is low because the generated workload is sparse, not because the calculation is wrong.

---

## Demo 2 — Strategy comparison

**Goal:** compare all three strategies on an equivalent workload with fresh state per run.

**Steps**

1. Choose `5` (Configure workload).
2. Press Enter to keep 20 jobs.
3. Press Enter to keep 5 servers.
4. Enter seed `84`.
5. Choose `4` (Compare all three strategies).
6. Choose `6` to exit.

**What to show**

```
Comparing all three strategies on 20 job(s) / 5 server(s)  [seed=84] ...

FirstFit
  Elapsed        : 59 tick(s)
  Completed      : 20  (100.0%)
  Rejected       : 0   (0.0%)
  Avg wait time  : 0.500 tick(s)  [sample n=20]
  Throughput     : 0.339 jobs/tick
  CPU / RAM / Storage / GPU : 10.8% / 18.6% / 2.6% / 11.7%

BestFit
  Elapsed        : 59 tick(s)
  Completed      : 20  (100.0%)
  Rejected       : 0   (0.0%)
  Avg wait time  : 0.250 tick(s)  [sample n=20]
  Throughput     : 0.339 jobs/tick
  CPU / RAM / Storage / GPU : 10.8% / 18.6% / 2.6% / 11.7%

LoadBalanced
  Elapsed        : 62 tick(s)
  Completed      : 20  (100.0%)
  Rejected       : 0   (0.0%)
  Avg wait time  : 0.650 tick(s)  [sample n=20]
  Throughput     : 0.323 jobs/tick
  CPU / RAM / Storage / GPU : 10.3% / 17.7% / 2.5% / 11.1%
```

**Talking points**

- All strategies receive the same seed, job specifications and server specifications, but each run builds a fresh `DataCenter`, job vector and `SimulationEngine`.
- Best Fit achieves the lowest average waiting time here (0.25 ticks); Load Balanced spreads work more evenly but delays completion slightly (62 vs 59 ticks).
- On seed 42 all three strategies produce identical metrics. That is expected for a sparse workload with no scheduling contention; it is not evidence that the strategies are the same implementation. Seed 84 deliberately creates contention and exposes their different placement rules.

---

## Demo 3 — Constrained resources and rejection

**Goal:** show rejection of impossible jobs and `N/A` handling for an unavailable resource dimension.

**Steps**

1. Choose `5` (Configure workload).
2. Enter `50` for job count.
3. Enter `1` for server count.
4. Enter seed `42`.
5. Choose `4` (Compare all three strategies).
6. Choose `6` to exit.

**What to show**

```
Comparing all three strategies on 50 job(s) / 1 server(s)  [seed=42] ...

Every strategy:
  Submitted   : 50
  Completed   : 11  (22.0%)
  Rejected    : 39  (78.0%)
  Still active: 0
  Avg wait time  : 2.182 tick(s)  [sample n=11]
  Throughput     : 0.220 jobs/tick
  CPU     : 49.3%
  RAM     : 43.5%
  Storage : 7.5%
  GPU     : N/A
```

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

**Talking points**

- With one server, 39 of 50 jobs cannot fit even when the server is completely idle. They are marked `REJECTED` at their arrival tick instead of waiting forever.
- `GPU : N/A` is reported because the generated server has zero GPU capacity; the denominator for GPU resource-time is zero, so the metric is undefined rather than a division by zero.
- With only one server, all three strategies make the same choices, so their reports match.
- The run terminates normally: every job is either `COMPLETED` or `REJECTED`, and the 11 completed jobs release all reserved resources.
