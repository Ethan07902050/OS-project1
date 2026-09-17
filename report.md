# Experimental Results and Analysis

Results for the user-space process scheduler described in [README.md](README.md).
This document is the online-readable version of the original course report
([report.pdf](report.pdf)), extended with aggregate measurements recomputed from the raw captures in
[output/](output/).

---

## 1. Methodology

Each simulated process is a real forked child. It brackets its CPU burst with two custom system calls
([process.c:38-41](process.c#L38-L41)):

```c
syscall(GETTIME, &start_sec, &start_nsec);      // my_gettime  (439)
for (int i = 0; i < p->exec; i++) unit_time();  // the actual CPU work
syscall(GETTIME, &stop_sec,  &stop_nsec);
syscall(PRINTK, getpid(), start_sec, start_nsec, stop_sec, stop_nsec);  // my_print (440)
```

Measurement is therefore done **inside the kernel**, via `ktime_get_real()`, and reported through
`printk()` rather than stdout. This matters: stdout is buffered, goes through the C library, and would
itself perturb the timing of the process being measured. The kernel log is the ground truth; stdout is
used only to announce PIDs at fork time.

Each run produces two files in [output/](output/):

- `<CASE>_stdout.txt` — `<name> <pid>` per process, written at creation
- `<CASE>_dmesg.txt` — `[Project1] <pid> <start>.<ns> <stop>.<ns>` per process, written at completion

Twenty test cases were run: five per policy across FIFO, SJF, PSJF and RR, with 2–8 processes each.

---

## 2. Calibration of the time unit

The scheduler's clock is not wall-clock time — it counts calls to `unit_time()`, a busy-wait of
1,000,000 `volatile` increments ([process.c:11-14](process.c#L11-L14)). To compare measured results
against theoretical predictions, that unit first has to be converted into seconds.

`TIME_MEASUREMENT` runs ten processes of 500 units each, back to back
([output/TIME_MEASUREMENT_dmesg.txt](output/TIME_MEASUREMENT_dmesg.txt)):

| PID | Start (s) | Stop (s) | Duration for 500 units |
|---|---|---|---|
| 10006 | 1588336126.967038135 | 1588336128.164550479 | 1.198 s |
| 10007 | 1588336129.278898166 | 1588336130.508257443 | 1.229 s |
| 10008 | 1588336131.571484687 | 1588336132.773393058 | 1.202 s |
| 10009 | 1588336133.842792909 | 1588336135.042435265 | 1.200 s |
| 10010 | 1588336136.134664661 | 1588336137.368424483 | 1.234 s |
| 10011 | 1588336138.438564054 | 1588336139.632976086 | 1.194 s |
| 10012 | 1588336140.815173235 | 1588336141.980972826 | 1.166 s |
| 10013 | 1588336143.182824143 | 1588336144.367800076 | 1.185 s |
| 10014 | 1588336145.562525556 | 1588336146.710174011 | 1.148 s |
| 10015 | 1588336147.826560091 | 1588336149.036617500 | 1.210 s |

**Mean: 1.197 s per 500 units → 1 time unit ≈ 2.39 ms** (spread 1.148–1.234 s, ±3.6%).

The spread is itself informative: the busy-wait is not perfectly reproducible even on a pinned,
otherwise-idle CPU. Frequency scaling, cache state and interrupt handling all contribute. Any
comparison against theory therefore carries a few percent of irreducible noise before scheduling
overhead is even considered.

---

## 3. Theory vs. measured timeline

For four representative cases, each process's predicted start/end tick is compared against its
measured start/end tick (kernel timestamps converted to time units using the calibration above).
All values are in **time units**.

### `FIFO_3` — FIFO, 7 processes

| Process | Theory start | Theory end | Measured start | Measured end | End drift |
|---|---|---|---|---|---|
| P1 | 0 | 8000 | 0 | 13170 | +5170 |
| P2 | 8000 | 13000 | 13287 | 21396 | +8396 |
| P3 | 13000 | 16000 | 21571 | 26464 | +10464 |
| P4 | 16000 | 17000 | 26529 | 28137 | +11137 |
| P5 | 17000 | 18000 | 28159 | 29754 | +11754 |
| P6 | 18000 | 19000 | 29796 | 31352 | +12352 |
| P7 | 19000 | 23000 | 31359 | 37957 | +14957 |

**Average run-time difference: 2075.57 units** (≈ 5.0 s).

The end-drift column is the clearest illustration of the whole experiment: it grows monotonically
from +5170 to +14957 down the timeline. Each process inherits every delay incurred before it, so
lateness **accumulates** rather than staying constant. The per-process penalty is roughly steady;
what grows is the running total.

### `RR_2` — round-robin, 2 processes, 500-unit quantum

| Process | Theory start | Theory end | Measured start | Measured end | End drift |
|---|---|---|---|---|---|
| P1 | 600 | 8100 | 600 | 13980 | +5880 |
| P2 | 1100 | 9600 | 1414 | 17078 | +7478 |

**Average run-time difference: 6522.0 units** (≈ 15.6 s).

Only two processes, yet by far the worst drift of any case. The theoretical timeline spans ticks
600 → 9600, i.e. roughly 9000 units of CPU work, which a 500-unit quantum chops into about 18
slices. FIFO or SJF would have dispatched twice. Those ~16 extra context switches are the entire
difference, and they cost ~6500 units — on the order of **400 units (≈ 1 s) of drift per extra
switch**.

### `SJF_2` — SJF, 5 processes

| Process | Theory start | Theory end | Measured start | Measured end | End drift |
|---|---|---|---|---|---|
| P1 | 100 | 200 | 100 | 277 | +77 |
| P2 | 400 | 4400 | 639 | 6698 | +2298 |
| P3 | 200 | 400 | 281 | 632 | +232 |
| P4 | 4400 | 8400 | 6698 | 13159 | +4759 |
| P5 | 8400 | 15400 | 13159 | 24518 | +9118 |

**Average run-time difference: 1821.4 units** (≈ 4.4 s).

Note the execution order: P1 → P3 → P2 → P4 → P5. P3 (200 units) is selected ahead of P2 (4000
units) even though both are available — the shortest-job rule, visible directly in the timestamps.

### `PSJF_5` — PSJF, 5 processes (same workload as `SJF_2`)

| Process | Theory start | Theory end | Measured start | Measured end | End drift |
|---|---|---|---|---|---|
| P1 | 100 | 200 | 100 | 269 | +69 |
| P2 | 400 | 4400 | 610 | 6937 | +2537 |
| P3 | 200 | 400 | 272 | 610 | +210 |
| P4 | 4400 | 8400 | 7087 | 14317 | +5917 |
| P5 | 8400 | 15400 | 14317 | 27295 | +11895 |

**Average run-time difference: 2348.4 units** (≈ 5.6 s).

This case is a useful control. Its theoretical timeline is **identical** to `SJF_2`'s, because in
this workload no shorter job ever arrives while a longer one is running — so PSJF never actually
preempts and degenerates into SJF. That prediction is confirmed independently in §4, where `PSJF_5`
is the one PSJF case with a span ratio of 0.99× (no overlapping spans, i.e. no preemption). The
measured drift is nonetheless ~29% worse than `SJF_2`, which is the price of PSJF re-evaluating its
choice on every tick even when the choice does not change.

### Summary

| Case | Policy | Average difference (units) | ≈ wall-clock |
|---|---|---|---|
| `SJF_2` | SJF, non-preemptive | 1821.4 | ≈ 4.4 s |
| `FIFO_3` | FIFO, non-preemptive | 2075.6 | ≈ 5.0 s |
| `PSJF_5` | PSJF, preemptive | 2348.4 | ≈ 5.6 s |
| `RR_2` | RR, 500-unit quantum | **6522.0** | ≈ 15.6 s |

**In every case, measured completion exceeds the theoretical prediction. There is no case where it
does not.** The sources:

1. **Context-switch cost.** Every switch costs the parent two `sched_setscheduler()` syscalls (demote
   outgoing, promote incoming) plus the kernel's own run-queue work.
2. **The scheduler's own execution.** The parent runs a full loop iteration — arrival scan, policy
   decision, and its own `unit_time()` — for every tick.
3. **Measurement overhead.** `printk()` and the syscall entry/exit path sit on the measured boundary.

The ordering of the four cases is itself the result: drift rises with how often a policy switches.
FIFO and SJF switch only on completion; PSJF re-decides every tick but rarely acts; RR switches on a
fixed clock regardless of what the workload is doing, and drifts roughly three times as far.
**RR pays for fairness in context switches, and context switches are what cost time here.**

## 4. Aggregate across all 20 runs

Recomputed directly from `output/*_dmesg.txt`. *Makespan* is last completion minus first start.
*Σ spans* is the sum of each process's own start→stop interval.

| Case | Policy | Procs | Makespan | Σ spans | Ratio |
|---|---|---|---|---|---|
| `FIFO_1` | FIFO | 5 | 10.99 s | 10.98 s | 1.00× |
| `FIFO_2` | FIFO | 4 | 391.02 s | 390.96 s | 1.00× |
| `FIFO_3` | FIFO | 7 | 98.69 s | 97.58 s | 0.99× |
| `FIFO_4` | FIFO | 4 | 13.74 s | 13.65 s | 0.99× |
| `FIFO_5` | FIFO | 7 | 96.58 s | 95.70 s | 0.99× |
| `SJF_1` | SJF | 4 | 58.11 s | 58.11 s | 1.00× |
| `SJF_2` | SJF | 5 | 63.49 s | 63.46 s | 1.00× |
| `SJF_3` | SJF | 8 | 139.53 s | 139.36 s | 1.00× |
| `SJF_4` | SJF | 5 | 42.82 s | 42.73 s | 1.00× |
| `SJF_5` | SJF | 4 | 12.66 s | 12.62 s | 1.00× |
| `PSJF_1` | PSJF | 4 | 99.57 s | 201.54 s | **2.02×** |
| `PSJF_2` | PSJF | 5 | 43.78 s | 60.18 s | **1.37×** |
| `PSJF_3` | PSJF | 4 | 14.41 s | 20.62 s | **1.43×** |
| `PSJF_4` | PSJF | 4 | 59.93 s | 64.15 s | 1.07× |
| `PSJF_5` | PSJF | 5 | 70.71 s | 70.31 s | 0.99× |
| `RR_1` | RR | 5 | 11.05 s | 11.04 s | 1.00× |
| `RR_2` | RR | 2 | 42.84 s | 75.51 s | **1.76×** |
| `RR_3` | RR | 6 | 128.41 s | 513.53 s | **4.00×** |
| `RR_4` | RR | 7 | 105.04 s | 391.22 s | **3.72×** |
| `RR_5` | RR | 7 | 97.88 s | 372.05 s | **3.80×** |

### What the ratio column proves

This column is a **behavioural fingerprint of preemption**, derived from nothing but timestamps:

- **A ratio of 1.00 means no process was ever preempted.** Each process starts only after the
  previous one finished, so the spans tile the timeline without overlap. Every FIFO and SJF case
  sits at 1.00× — exactly as the non-preemptive short-circuit in
  [`next_process()`](scheduling.c#L17-L18) requires. This is an independent confirmation that the
  policies behave as implemented.
- **A ratio above 1.00 means overlapping spans, which only preemption can produce.** A preempted
  process remains inside its own start→stop window while another process holds the CPU, so the spans
  double-count the same wall-clock seconds. `RR_3` at 4.00× means that, on average, four processes
  were simultaneously "in flight" — started but not finished.
- **RR's ratio scales with process count**: 1.00× at 5 short processes, but 3.7–4.0× at 6–7. With a
  fixed quantum, the number of processes interleaved in the queue is exactly what drives the ratio.
- **`RR_1` at 1.00× is not a contradiction.** Its five processes each run ~900 units, comfortably
  past the 500-unit quantum — yet none was preempted. The reason is the second half of the quantum
  condition at [scheduling.c:141](scheduling.c#L141): `... && !is_empty(&q)`. The quantum only
  preempts when someone is actually waiting. Here the ready times are staggered so that each process
  arrives about when its predecessor finishes (the measured spans are back-to-back, with gaps of
  1–9 ms), leaving the queue empty every time the quantum expires, and RR degenerates into FIFO.
  `PSJF_5` at 0.99× is the analogous story: no shorter job ever arrived mid-execution, so preemption
  never triggered — confirmed independently by its theoretical timeline being identical to `SJF_2`'s
  in §3.

This is also the practical trade-off the policies are meant to illustrate. RR's overlapping spans are
not waste — they are responsiveness. Every process makes progress early instead of waiting for all
its predecessors, at the cost of a longer individual turnaround and more switching overhead. FIFO and
SJF give the opposite bargain.

---

## 5. Caveats on the raw data

Stated plainly, since these are the captures as they were taken in 2020 and are kept unmodified:

- A few `dmesg` captures do not line up with their `stdout` counterparts. `RR_2` lists PIDs 9491/9492
  on stdout but 9549/9550 in `dmesg`; `PSJF_5` lists five PIDs on stdout, of which `dmesg` shows four
  plus one from an adjacent run. The captures were evidently taken with `dmesg | tail -n`, which can
  pick up lines from a neighbouring run. `SJF_3` similarly shows eight `dmesg` lines for seven
  processes.
- Consequence: **aggregate trends are sound** — the makespan/Σ-spans separation between preemptive and
  non-preemptive policies is far too large to be an artefact of one stray line — but individual
  PID-level rows in those specific files should not be over-read.
- The theoretical baselines in §3 are reproduced from the original report. The test-case **input
  files are not preserved** in this repository, so those predictions cannot be re-derived from
  scratch here — though the theory columns encode the inputs implicitly (`SJF_2`/`PSJF_5`, for
  instance, imply bursts of 100, 4000, 200, 4000 and 7000 units). §4, by contrast, is computed
  entirely from the raw `dmesg` captures and stands on its own.

---

## 6. Conclusion

Measured run times exceed the theoretical prediction in every scenario. The overhead is attributable
to context switching between processes and to the execution of the scheduler itself — both of which
raise total CPU usage and slow overall progress. The effect is cumulative along the timeline and
scales with the number of context switches a policy performs, which is why round-robin, whose switch
count is set by a fixed quantum rather than by process behaviour, drifts furthest from theory.

The wider lesson is that scheduling overhead is not a rounding error at this granularity. With a time
unit of ~2.4 ms, a policy that switches often spends a measurable fraction of the run inside the
scheduler rather than inside the workload — the same reason real kernels tune their quanta in
milliseconds rather than microseconds.
