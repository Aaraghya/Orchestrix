# Orchestrix — Demo Guide

This guide demonstrates every major Orchestrix CLI feature. Seeded workloads make the scenarios reproducible; outputs below are excerpts from the current build.

---

## Quick Start

Build and run from the repository root:

```bash
cmake -S . -B build -G Ninja
cmake --build build
./build/orchestrix.exe
```

On Windows the executable is `build\orchestrix.exe`.

The program prints the current workload settings and these options:

```text
  1. Run simulation - First Fit
     First compatible server with enough resources.
  2. Run simulation - Best Fit
     Compatible server with the lowest Best Fit score.
  3. Run simulation - Load Balanced
     Compatible server with the lowest projected utilization.
  4. Compare all three strategies
     Compare performance using the same generated workload.
  5. Configure workload (Job count / seed)
     Adjust workload generation settings.
  6. Server status display (First Fit run)
  7. Filter simulation event log by type
  8. Exit
Choice:
```

**Input conventions**

- Type an option number and press Enter.
- Option 5 keeps the current value when you enter a blank or invalid value.
- Options 1–4, 6 and 7 generate a fresh workload from the current settings.
- Options 1–3 use a paginated screen: `N` next, `P` previous, `Q` back to the menu, 20 records per page.
- Option 7 accepts `1`–`4` or `arrival`, `start`, `completion`, and `rejection`.
- The same settings and seed reproduce the same workload and results. Settings persist until changed or the program exits.

**Default workload**

| Setting | Default | Notes |
|---|---:|---|
| Jobs | 20 | Configurable 1–1000 |
| Servers | 5 | Configurable 1–100 |
| Seed | 42 | Configurable 0–4294967295 |
| Max arrival / duration | 50 / 10 ticks | Fixed for this build |

**Reading the results**

The Options 1–3 screen shows the strategy, jobs processed, jobs allocated (completed plus running), jobs rejected, `Average allocation time` (always `N/A`), utilization, and one row per job. A missing server is shown as `-`; statuses include `Allocated`, `Running`, `Rejected`, `Queued`, and `Pending`.

Option 4 adds elapsed ticks, waiting time, throughput, a queued/running breakdown, and a comparison summary. Percentages use submitted jobs as the denominator. In these detailed reports, `N/A` means no valid value (no samples or a zero denominator), not zero.

---

## Demo 1 — Individual strategy run (Options 1–3)

**Goal:** run one strategy and read its paginated results.

**Steps**

1. Keep the defaults: 20 jobs, 5 servers, seed 42.
2. Choose `1` (First Fit).
3. Review the page; use `N` / `P` if needed.
4. Choose `Q`, then `6` to exit.

**What to observe**

- The summary reports processed, allocated, and rejected jobs plus utilization.
- Rejected jobs show no server ID.
- For seed 42, Options 2 and 3 produce the same summary values, but their server assignments differ.

**Sample output (excerpt)**

```text
Strategy: First Fit
Jobs processed: 20
Jobs allocated: 18
Jobs rejected: 2
Average allocation time: N/A
Resource utilization: CPU: 9.9% | RAM: 8.2% | Storage: 3.2% | GPU: 7.2%

Job ID    Server ID    Status
--------------------------------
job-0     srv-3        Allocated
job-1     srv-0        Allocated
job-4     -            Rejected
...
Showing results 1–20 of 20
[N] Next page | [P] Previous page | [Q] Return to menu
```

---

## Demo 2 — Strategy comparison (Option 4)

**Goal:** compare all three strategies on one workload.

**Steps**

1. Choose `5`, press Enter twice to keep 20 jobs and 5 servers, then enter seed `84`.
2. Choose `4` (Compare all three strategies).

**What to observe**

- Each strategy starts with fresh state on the same workload.
- Waiting time and elapsed ticks can differ even when every job completes; the final table summarizes outcomes.

| Metric | FirstFit | BestFit | LoadBalanced |
|---|---:|---:|---:|
| Elapsed ticks | 59 | 59 | 62 |
| Completed / rejected | 20 / 0 | 20 / 0 | 20 / 0 |
| Avg wait time | 0.500 | 0.250 | 0.650 |
| Throughput (jobs/tick) | 0.339 | 0.339 | 0.323 |
| CPU | 10.8% | 10.8% | 10.3% |
| RAM | 18.6% | 18.6% | 17.7% |
| Storage | 2.6% | 2.6% | 2.5% |
| GPU | 11.7% | 11.7% | 11.1% |

```text
  Job Outcome Summary (Final Job Counts)
  Strategy        Completed  Rejected   Queued  Running
  FirstFit               20         0        0        0
  BestFit                20         0        0        0
  LoadBalanced           20         0        0        0
```

---

## Demo 3 — Constrained resources and rejection

**Goal:** show permanent rejection and `N/A` utilization.

**Steps**

1. Choose `5`, then enter `50` jobs, `1` server, and seed `42`.
2. Choose `4` (Compare all three strategies).

**What to observe**

- Jobs that can never fit are rejected as they arrive.
- With one server, all three strategies tie; GPU is `N/A` because the server has no GPU capacity.

| Metric | Value |
|---|---:|
| Elapsed ticks | 50 |
| Completed | 11 (22.0%) |
| Rejected | 39 (78.0%) |
| Avg wait time | 2.182 ticks (n=11) |
| Throughput | 0.220 jobs/tick |
| CPU / RAM / Storage | 49.3% / 43.5% / 7.5% |
| GPU | N/A |

All three rows in the final summary show `11` completed and `39` rejected.

---

## Demo 4 — Workload configuration and reproducibility (Option 5)

**Goal:** change the workload and show that a seed reproduces a run.

**Steps**

1. Choose `5`, then enter `12` jobs, `4` servers, and seed `123`.
2. Choose `1`, review the results, and press `Q`.
3. Choose `1` again; the allocation records should match the first run.
4. Choose `5`, keep 12 jobs and 4 servers with Enter, enter seed `124`, then run `1` again.

**What to observe**

- Option 5 shows each current value in its prompt.
- The same seed produces identical records; changing only the seed can change counts and utilization.

```text
  New job count   [1-1000, Enter to keep 20]: 12
  New server count [1-100, Enter to keep 5]: 4
  New seed        [0-4294967295, Enter to keep 42]: 123
```

| Seed (12 jobs / 4 servers) | Allocated | Rejected | CPU | RAM | Storage | GPU |
|---:|---:|---:|---:|---:|---:|---:|
| 123 | 5 | 7 | 3.7% | 7.9% | 1.8% | N/A |
| 124 | 11 | 1 | 15.5% | 11.7% | 5.2% | 0.0% |

---

## Demo 5 — Server status display (Option 6)

**Goal:** show how available resources change during a First Fit run.

**Steps**

1. Choose `5`, then enter `6` jobs, `3` servers, and seed `7`.
2. Choose `6` (Server status display), then `8` to exit.

**What to observe**

- Tick 0 shows every server at full capacity.
- A job start reduces available resources; a release restores them, and the final snapshot is fully available again.

```text
  [tick 0 - initial state]
  Server  Type          Cap.CPU Avl.CPU Cap.RAM Avl.RAM Cap.Stor  Avl.Stor  Cap.GPU Avl.GPU
  srv-0   GPUServer     10.0    10.0    33.0    33.0    166.0     166.0     2.0     2.0
  srv-1   ComputeServer 13.0    13.0    103.0   103.0   324.0     324.0     0.0     0.0
  srv-2   StorageServer 4.0     4.0     13.0    13.0    2077.0    2077.0    0.0     0.0

  [tick 3 - job job-4 started on srv-2]
  srv-2   StorageServer 4.0     2.0     13.0    11.0    2077.0    1650.0    0.0     0.0

  [tick 10 - job job-4 released resources from srv-2]
  srv-2   StorageServer 4.0     4.0     13.0    13.0    2077.0    2077.0    0.0     0.0
```

The full timeline also includes snapshots at ticks 4, 11, 13, 18, 35, 42, 44, 45, 51, and 53.

---

## Demo 6 — Filter the event log (Option 7)

**Goal:** inspect one event type from a completed First Fit run.

**Steps**

1. Restore the defaults with Option 5: 20 jobs, 5 servers, seed 42.
2. Choose `7`, then `4` (Job Rejections), or type `rejection`.
3. Choose `8` to exit.

**What to observe**

- Only matching events are listed with tick, job ID, server ID, and detail; the total count confirms the matches.

```text
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

| Step | Feature | Inputs | Emphasize |
|---:|---|---|---|
| 1 | Individual run | `1`, `Q` | Paginated screen and rejected rows |
| 2 | Strategy comparison | `5`, Enter, Enter, `84`, `4` | Fresh state and the summary table |
| 3 | Constrained resources | `5`, `50`, `1`, `42`, `4` | Rejection and `GPU : N/A` |
| 4 | Reproducibility | `5`, `12`, `4`, `123`, `1`, `Q`, `1`, `Q`; then `5`, Enter, Enter, `124`, `1`, `Q` | Identical rerun, then a changed workload |
| 5 | Server status | `5`, `6`, `3`, `6`, `8` | Reservation and release over time |
| 6 | Event filtering | `5`, `20`, `5`, `42`, `7`, `4` | Event log as the source of report data |

Workload settings persist between menu choices, so each step sets them explicitly. Use `8` whenever you want to leave the program.
