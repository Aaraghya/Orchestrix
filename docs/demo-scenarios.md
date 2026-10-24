# Orchestrix — Demo Guide

This guide covers every major feature of the Orchestrix command-line interface. Each scenario is reproducible: the workload generator is seeded, the simulation is deterministic, and every output shown below was captured from the current build.

Use it as a run sheet during the project demonstration. Explanations are included so you can talk about what the program is doing, not just read numbers aloud.

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
- The same settings and seed always produce the same workload and the same results.

**Default workload**

| Setting | Default | Notes |
|---|---:|---|
| Jobs | 20 | Configurable 1–1000 |
| Servers | 5 | Configurable 1–100 |
| Seed | 42 | Configurable 0–4294967295 |
| Max arrival | 50 ticks | Fixed for this build |
| Max duration | 10 ticks | Fixed for this build |

**Reading a report**

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

**Goal:** run one strategy and read its report.

**Steps**

1. Choose `1` (First Fit).
2. Leave the defaults unchanged: 20 jobs, 5 servers, seed 42.
3. Choose `6` to exit.

**Verified output**

```
-------------------------------------------------------
  Strategy  : FirstFit
  Status    : Completed normally
  Elapsed   : 54 tick(s)

  Job Outcomes
    Submitted   : 20
    Completed   : 18  (90.0%)
    Rejected    : 2  (10.0%)
    Cancelled   : 0
    Failed      : 0
    Still active: 0
      Queued    : 0
      Running   : 0

  Performance
    Avg wait time  : 0.000 tick(s)  [sample n=18]
    Throughput     : 0.333 jobs/tick

  Resource Utilization (resource-time accounting)
    CPU     : 9.9%
    RAM     : 8.2%
    Storage : 3.2%
    GPU     : 7.2%
-------------------------------------------------------
```

Options 2 and 3 produce identical values for this seed because the workload has no scheduling contention:

| Option | Strategy | Elapsed | Completed | Rejected | Avg wait | Throughput |
|---|---|---:|---:|---:|---:|---:|
| 1 | FirstFit | 54 | 18 | 2 | 0.000 | 0.333 |
| 2 | BestFit | 54 | 18 | 2 | 0.000 | 0.333 |
| 3 | LoadBalanced | 54 | 18 | 2 | 0.000 | 0.333 |

All three also report the same utilization: CPU 9.9%, RAM 8.2%, Storage 3.2%, GPU 7.2%.

**Talking points**

- `Completed normally` means every job reached a terminal state before the 100,000-tick safeguard.
- Two jobs are rejected at their arrival tick because their requirements exceed the total capacity of every compatible server. They never enter the waiting queue.
- `Elapsed = 54` is the tick at which the last job completed.
- `Throughput = 18 / 54 = 0.333 jobs/tick`.
- The average wait uses only the 18 jobs that started; every one of them started on its arrival tick.
- Utilization is low because this workload is sparse. It is a resource-time average over the entire run, not an instantaneous load reading.
- Identical strategy results at seed 42 are expected here; Demo 2 creates contention and separates them.

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

**Talking points**

- All strategies receive the same server specifications and job specifications. Each run builds a fresh `DataCenter`, job vector, `Scheduler`, and `SimulationEngine`, so no allocation state leaks between runs.
- Completed and rejected counts tie here because rejection depends on whether a job can ever fit, not on the placement policy.
- Waiting time exposes the placement difference: Best Fit gives the lowest average wait in this run, while Load Balanced delays the final completion to tick 62.
- The summary table is a quick final-count check. `Queued` and `Running` are zero because the simulation finished normally with all jobs terminal.
- At seed 42 the three strategies tie; seed 84 deliberately creates contention. Use both to show that the strategies are different implementations, not copies.

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

**Talking points**

- A job is rejected at arrival only when its requirements cannot fit within the total capacity of any compatible server, even if that server were completely idle. A temporarily busy server does not cause permanent rejection.
- 39 jobs cannot fit on the single generated server, so they are rejected immediately instead of waiting forever.
- `GPU : N/A` appears because the generated server has zero GPU capacity. The utilization denominator is zero, so the metric is correctly undefined rather than a division by zero.
- With one server there is no placement choice, so First Fit, Best Fit, and Load Balanced necessarily produce the same report.
- The run still terminates normally: 11 jobs complete, 39 are rejected, and all reserved resources are released.

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

**Talking points**

- The three editable values are job count (1–1000), server count (1–100), and seed (unsigned 32-bit).
- Blank input keeps the current value. Invalid or out-of-range input behaves the same way and prints a `Keeping ...` message.
- `Max arrival` and `Max duration` are displayed for context but are not editable through the CLI.
- Options 1–4 and 7–8 generate a fresh workload from these settings on every run. Settings persist until changed or until the program is restarted.
- The same job count, server count, and seed always regenerate the same workload, which is what makes these demos repeatable.
- Option 4 builds one set of specifications and reuses it for all three strategies; it does not generate a different workload per strategy.

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

**Talking points**

- Option 7 does not reuse a previous report. It reruns First Fit using the current workload settings.
- The table is reconstructed by replaying job start and resource release events from the run's event log. It starts from each server's total capacity and does not read live allocation state.
- `Cap` is total capacity; `Avl` is what remains available. The difference is the amount currently reserved.
- At tick 3, job-4 consumes 2 CPU cores, 2 GB RAM, and 427 GB storage on `srv-2`; those values return at tick 10, showing a 7-tick reservation.
- Rows are ordered by server ID, and values are printed to one decimal place.
- Output length grows with the number of events, so use a small configured workload during the live demo.

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

**Talking points**

- Option 8 runs First Fit with the current settings, then filters that run's in-memory event log. It does not modify the simulation or rerun it per filter.
- The four filters correspond to the four lifecycle points: arrival, start, completion, and rejection.
- At the prompt you can also type `arrival`, `start`, `completion`, `rejection`, or the exact event names such as `JOB_ARRIVED`.
- Events are printed in chronological order. The server column shows `-` when an event has no server, as with arrivals and rejections.
- These two rejections are the same jobs counted as rejected in Demo 1. Choosing filter 1 would list all 20 arrivals; filter 2 would also show the `Finish tick=...` detail for each start.
- An invalid filter choice prints a message and returns to the main menu without printing an event table.

---

## Suggested live demo order

| Step | Feature | Inputs | What to emphasize |
|---:|---|---|---|
| 1 | Individual run | `1`, `6` | Baseline report and metric meanings |
| 2 | Strategy comparison | `5`, Enter, Enter, `84`, `4` | Fresh state per strategy and the summary table |
| 3 | Constrained resources | `5`, `50`, `1`, `42`, `4` | Rejection at arrival and `GPU : N/A` |
| 4 | Server status | `5`, `6`, `3`, `7`, `7` | Resource reservation and release over time |
| 5 | Event filtering | `8`, `4` | Event log as the source of report data |

Workload settings persist between menu choices, so set them explicitly at each step or restart between demos.

---

## Verification

Verified on 2026-10-25 against the current source:

- `cmake --build build` — build up to date, no errors.
- `ctest --test-dir build --output-on-failure` — **10/10 tests passed** (212 unit tests across six milestone executables, plus four CLI smoke tests).
- All reports and event tables in this guide were produced by running the executable with the stated inputs. Repeating a scenario with the same settings reproduces the same output.
