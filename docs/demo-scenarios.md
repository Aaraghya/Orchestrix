# Orchestrix — Demo Guide

This guide covers every major feature of the Orchestrix command-line interface. Each scenario is reproducible: the workload generator is seeded, the simulation is deterministic, and we captured every output shown below from the current build.

---

## Before you start

Build and run from the repository root:

```bash
cmake -S . -B build -G Ninja
cmake --build build
./build/orchestrix.exe
```

On Windows the executable is `build\orchestrix.exe`. Run the tests with:

```bash
ctest --test-dir build --output-on-failure
```

The menu has eight options:

```
------------------------------------------------------------
  MAIN MENU
------------------------------------------------------------
  1. Run simulation - First Fit
  2. Run simulation - Best Fit
  3. Run simulation - Load Balanced
  4. Compare all three strategies
  5. Configure workload (job count / seed)
  6. Exit
  7. Server status display (First Fit run)
  8. Filter simulation event log by type
------------------------------------------------------------
```

**Input conventions**

- Type the option number and press Enter.
- Option 5 accepts blank input to keep the current value. Invalid or out-of-range input is also treated as "keep".
- Options 1–4 and 7–8 always generate a fresh workload from the current settings.
- Options 1–3 end with a paginated results screen: `N` for the next page, `P` for the previous page, and `Q` to return to the menu.
- The same settings and seed always produce the same workload and the same results.

**Default workload**

| Setting | Default | Notes |
|---|---:|---|
| Jobs | 20 | Configurable 1–1000 |
| Servers | 5 | Configurable 1–100 |
| Seed | 42 | Configurable 0–4294967295 |
| Max arrival | 50 ticks | Fixed for this build |
| Max duration | 10 ticks | Fixed for this build |

**Reading the paginated results screen (Options 1–3)**

| Field | Meaning |
|---|---|
| `Strategy` | Display name of the strategy that produced the run. |
| `Jobs processed` | Total jobs in the generated workload. |
| `Jobs allocated` | Jobs that completed or were still running at the stop tick. |
| `Jobs rejected` | Jobs rejected as permanently infeasible. |
| `Average allocation time` | Always `N/A`: allocation time is not measured in this build. |
| `Resource utilization` | Resource-time utilization per dimension, computed over the whole run. |
| `Showing results X–Y of N` | Records on the current page and the total record count. |
| `[N]` / `[P]` / `[Q]` | Next page, previous page, or return to the main menu. |

Each page shows at most 20 job records, so the 20-job default workload fits on a single page. Only Options 1–3 use this paginated screen; Options 4, 7, and 8 keep their existing detailed reports.

**Reading the detailed reports (Option 4)**

| Field | Meaning |
|---|---|
| `Completed` / `Rejected` | Final job counts. Percentages use submitted jobs as the denominator. |
| `Still active` | Jobs still `PENDING`, `QUEUED`, or `RUNNING` at the stop tick. `Queued` and `Running` are its subsets. |
| `Avg wait time` | Mean of `startTick − arrivalTick` over jobs that actually started. `n` is the number of samples. |
| `Throughput` | `completed / elapsedTicks` jobs per tick. |
| `CPU`, `RAM`, `Storage`, `GPU` | Resource-time utilization: allocated resource-ticks divided by capacity-ticks over the whole run. |
| `N/A` | The metric has no valid denominator, for example zero GPU capacity. It is not a zero result. |

---

## Demo 1 — Individual strategy run (Options 1–3)

**Goal:** run one strategy and read its paginated results screen.

**Steps**

1. Choose `1` (First Fit).
2. Leave the defaults unchanged: 20 jobs, 5 servers, seed 42.
3. Review the results page. This 20-job workload fits on one page; larger workloads show at most 20 records per page, with `N` for next, `P` for previous, and `Q` to return to the menu.
4. Choose `6` to exit.

**Verified output**

```
============================================================
                  ORCHESTRIX SIMULATION
============================================================

Strategy: First Fit
Jobs processed: 20
Jobs allocated: 18
Jobs rejected: 2
Average allocation time: N/A
Resource utilization: CPU: 9.9% | RAM: 8.2% | Storage: 3.2% | GPU: 7.2%

------------------------------------------------------------
                   ALLOCATION DETAILS
------------------------------------------------------------

Job ID    Server ID    Status
--------------------------------
job-0     srv-3        Allocated
job-1     srv-0        Allocated
job-2     srv-0        Allocated
job-3     srv-2        Allocated
job-4     -            Rejected
job-5     srv-2        Allocated
job-6     srv-2        Allocated
job-7     srv-4        Allocated
job-8     srv-0        Allocated
job-9     srv-1        Allocated
job-10    srv-1        Allocated
job-11    srv-0        Allocated
job-12    srv-2        Allocated
job-13    srv-2        Allocated
job-14    srv-1        Allocated
job-15    -            Rejected
job-16    srv-4        Allocated
job-17    srv-0        Allocated
job-18    srv-1        Allocated
job-19    srv-0        Allocated

------------------------------------------------------------
Showing results 1–20 of 20
[N] Next page | [P] Previous page | [Q] Return to menu
> Q
```

Options 2 and 3 show the same summary values for this seed because the workload has no scheduling contention: 20 processed, 18 allocated, 2 rejected, and the same utilization. The detailed timing metrics (elapsed ticks, average wait, throughput) remain available in the Option 4 comparison report (Demo 2).

---

## Demo 2 — Strategy comparison (Option 4)

**Goal:** compare all three strategies on one workload and show the compact outcome summary.

**Steps**

1. Choose `5` (Configure workload).
2. Press Enter to keep 20 jobs.
3. Press Enter to keep 5 servers.
4. Enter seed `84`.
5. Choose `4` (Compare all three strategies).
6. Choose `6` to exit.

**Verified output**

The comparison starts with:

```
  Comparing all three strategies on 20 job(s) / 5 server(s)  [seed=84] ...
```

Key values condensed from the three full reports:

| Metric | FirstFit | BestFit | LoadBalanced |
|---|---:|---:|---:|
| Elapsed ticks | 59 | 59 | 62 |
| Completed | 20 | 20 | 20 |
| Rejected | 0 | 0 | 0 |
| Avg wait time | 0.500 | 0.250 | 0.650 |
| Throughput (jobs/tick) | 0.339 | 0.339 | 0.323 |
| CPU | 10.8% | 10.8% | 10.3% |
| RAM | 18.6% | 18.6% | 17.7% |
| Storage | 2.6% | 2.6% | 2.5% |
| GPU | 11.7% | 11.7% | 11.1% |

The report ends with the compact job outcome summary:

```
-------------------------------------------------------
  Job Outcome Summary (Final Job Counts)
-------------------------------------------------------
  Strategy        Completed  Rejected   Queued  Running
  -----------------------------------------------------
  FirstFit               20         0        0        0
  BestFit                20         0        0        0
  LoadBalanced           20         0        0        0
-------------------------------------------------------
```
---

## Demo 3 — Constrained resources and rejection

**Goal:** show permanent rejection and an undefined (`N/A`) resource metric.

**Steps**

1. Choose `5` (Configure workload).
2. Enter `50` for job count.
3. Enter `1` for server count.
4. Enter seed `42`.
5. Choose `4` (Compare all three strategies).
6. Choose `6` to exit.

**Verified output**

With one server, all three strategies make the same decisions. The key values are:

| Metric | Value |
|---|---:|
| Elapsed ticks | 50 |
| Submitted | 50 |
| Completed | 11 (22.0%) |
| Rejected | 39 (78.0%) |
| Still active | 0 |
| Avg wait time | 2.182 ticks (sample n=11) |
| Throughput | 0.220 jobs/tick |
| CPU | 49.3% |
| RAM | 43.5% |
| Storage | 7.5% |
| GPU | N/A |

The compact summary table confirms the tie:

```
-------------------------------------------------------
  Job Outcome Summary (Final Job Counts)
-------------------------------------------------------
  Strategy        Completed  Rejected   Queued  Running
  -----------------------------------------------------
  FirstFit               11        39        0        0
  BestFit                11        39        0        0
  LoadBalanced           11        39        0        0
-------------------------------------------------------
```
---

## Demo 4 — Workload configuration (Option 5)

**Goal:** change job count, server count, and seed before a run.

**Steps**

1. Choose `5`.
2. Enter `12` for job count.
3. Enter `4` for server count.
4. Enter seed `123`.
5. Choose `6` to exit (or continue with another demo).

**Verified output**

```
  New job count   [1-1000, Enter to keep 20]: 12
  New server count [1-100, Enter to keep 5]: 4
  New seed        [0-4294967295, Enter to keep 42]: 123

  Updated workload settings:
    Jobs       : 12
    Servers    : 4
    Seed       : 123
    Max arrival: 50 tick(s)
    Max dur.   : 10 tick(s)
```
---

## Demo 5 — Server status display (Option 7)

**Goal:** show how available resources on each server change during a First Fit run.

Option 7 always runs First Fit. It is best demonstrated with a small workload because it prints a full table after every job start and every resource release.

**Steps**

1. Choose `5`.
2. Enter `6` jobs, `3` servers, and seed `7`.
3. Choose `7` (Server status display).
4. Choose `6` to exit.

**Verified output**

```
  Running First Fit for server status display on 6 job(s) / 3 server(s)  [seed=7] ...

=======================================================
  Server Resource Status
=======================================================
  (Avl = available; Cap = total capacity; units: cores / GB / devices)

  [tick 0 — initial state]
  Server  Type          Cap.CPU Avl.CPU Cap.RAM Avl.RAM Cap.Stor  Avl.Stor  Cap.GPU Avl.GPU
  ------------------------------------------------------------------------------------------
  srv-0   GPUServer     10.0    10.0    33.0    33.0    166.0     166.0     2.0     2.0
  srv-1   ComputeServer 13.0    13.0    103.0   103.0   324.0     324.0     0.0     0.0
  srv-2   StorageServer 4.0     4.0     13.0    13.0    2077.0    2077.0    0.0     0.0

  [tick 3 — job job-4 started on srv-2]
  Server  Type          Cap.CPU Avl.CPU Cap.RAM Avl.RAM Cap.Stor  Avl.Stor  Cap.GPU Avl.GPU
  ------------------------------------------------------------------------------------------
  srv-0   GPUServer     10.0    10.0    33.0    33.0    166.0     166.0     2.0     2.0
  srv-1   ComputeServer 13.0    13.0    103.0   103.0   324.0     324.0     0.0     0.0
  srv-2   StorageServer 4.0     2.0     13.0    11.0    2077.0    1650.0    0.0     0.0

  [tick 10 — job job-4 released resources from srv-2]
  Server  Type          Cap.CPU Avl.CPU Cap.RAM Avl.RAM Cap.Stor  Avl.Stor  Cap.GPU Avl.GPU
  ------------------------------------------------------------------------------------------
  srv-0   GPUServer     10.0    10.0    33.0    33.0    166.0     166.0     2.0     2.0
  srv-1   ComputeServer 13.0    13.0    103.0   103.0   324.0     324.0     0.0     0.0
  srv-2   StorageServer 4.0     4.0     13.0    13.0    2077.0    2077.0    0.0     0.0
```

This is an excerpt. The full report prints one snapshot after each `JOB_STARTED` and `RESOURCE_RELEASED` event. For this run the remaining snapshots occur at ticks 4, 11, 13, 18, 35, 42, 44, 45, 51, and 53, ending with every server fully available again.

---

## Demo 6 — Filter the event log (Option 8)

**Goal:** inspect one event type from a completed First Fit run.

**Steps**

1. Start from the defaults (20 jobs, 5 servers, seed 42). If you changed them, restore them with Option 5 or restart the program.
2. Choose `8` (Filter simulation event log by type).
3. Choose `4` (Job Rejections).
4. Choose `6` to exit.

**Verified output**

The filter menu offers four choices:

```
------------------------------------------------------------
  FILTER EVENT LOG BY TYPE
------------------------------------------------------------
  1. Job Arrivals    (JOB_ARRIVED)
  2. Job Starts      (JOB_STARTED)
  3. Job Completions (JOB_COMPLETED)
  4. Job Rejections  (JOB_REJECTED)
------------------------------------------------------------
  Choice (1-4): 4
```

Choosing `4` prints:

```
------------------------------------------------------------
  Simulation Events: JOB_REJECTED
------------------------------------------------------------
  Tick    Job ID      Server ID   Detail
  ----------------------------------------------------------
  12      job-15      -           No compatible server has sufficient total capacity
  13      job-4       -           No compatible server has sufficient total capacity
------------------------------------------------------------
  Total matching events: 2
------------------------------------------------------------
```
---

## Suggested live demo order

| Step | Feature | Inputs | What to emphasize |
|---:|---|---|---|
| 1 | Individual run | `1`, `Q`, `6` | Paginated results screen, navigation, and summary fields |
| 2 | Strategy comparison | `5`, Enter, Enter, `84`, `4` | Fresh state per strategy and the summary table |
| 3 | Constrained resources | `5`, `50`, `1`, `42`, `4` | Rejection at arrival and `GPU : N/A` |
| 4 | Server status | `5`, `6`, `3`, `7`, `7` | Resource reservation and release over time |
| 5 | Event filtering | `8`, `4` | Event log as the source of report data |

Workload settings persist between menu choices, so set them explicitly at each step or restart between demos.

---
