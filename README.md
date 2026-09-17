# A CPU Scheduler Built on Real Linux Processes

An implementation of four classic CPU scheduling algorithms — **FIFO** (First-In First-Out),
**RR** (Round-Robin), **SJF** (Shortest Job First) and **PSJF** (Preemptive Shortest Job First,
also known as Shortest Remaining Time First).

The distinguishing point is that this is **not a simulation**. Most implementations of these
algorithms keep an imaginary clock and an array of imaginary processes. Here, every process is a real
Linux process doing real CPU work, and the program enforces its decisions on the live operating
system — deciding, moment to moment, which process the kernel is allowed to run.

**Why do it the hard way?** Because a simulation can only ever confirm the arithmetic it was built
from. It cannot tell you what scheduling actually *costs*. By running the algorithms against the real
kernel and measuring with nanosecond timestamps, this project shows something a simulation never
could: every run finishes measurably later than theory predicts, the gap accumulates down the
timeline, and the algorithm that switches most often pays about three times the penalty of the ones
that switch least. Scheduling overhead stops being a footnote and becomes a number.

Making that measurement trustworthy required extending the Linux kernel itself with two new system
calls, which meant patching and recompiling the kernel from source.

> NTU CSIE — Operating Systems (Spring 2020), Project 1.
> [Detailed results and analysis](report.md) · [Original course report](report.pdf) · [Demo video](demo.mp4)

---

## Highlights

- **Four scheduling algorithms**, two non-preemptive (FIFO, SJF) and two preemptive (PSJF, RR).
- **Real processes, not a simulation.** The scheduler's decisions are carried out by the Linux
  kernel on actual running processes.
- **Extended the operating system kernel.** Two custom system calls were added to Linux 5.6.7,
  requiring source patches and a full kernel rebuild.
- **Isolated the experiment from the OS.** The workload is confined to one CPU core and the
  scheduler to another, so Linux's own scheduler cannot interfere with the results.
- **Measured, not assumed.** 20 experiments comparing the theoretical timeline against the real one.
  Round-Robin drifted **~3× further from theory** than the non-preemptive algorithms — the
  measurable price of its fixed time slice.

## Skills

- **Operating systems** — CPU scheduling · preemption · context switching · process management
- **Linux kernel** — system call implementation · kernel patching and compilation · kernel/user-space boundary
- **Systems programming** — C · POSIX process API · concurrency and synchronization · CPU affinity
- **Engineering practice** — performance measurement · experimental analysis · technical writing

---

## What it actually does

Given a list of processes — each with an arrival time and an amount of work — the program runs them
under the chosen algorithm and reports when each one really started and finished.

Take four processes:

| Process | Arrives at | Work |
|---|---|---|
| P1 | 0 | 400 |
| P2 | 100 | 200 |
| P3 | 200 | 600 |
| P4 | 300 | 100 |

Under **FIFO**, they run in arrival order. P4 needs only 100 units, but it arrived last, so it waits
behind a 600-unit process:

```
  P1  ████████████████████                                                 0 →  400
  P2                      ██████████                                     400 →  600
  P3                                ██████████████████████████████       600 → 1200
  P4                                                              █████ 1200 → 1300
      └────────────────────── total time: 1300 ───────────────────────┘
      average turnaround (arrival → completion): 725
```

Under **SJF**, the shortest available process goes first. The same P4 now slips in early:

```
  P1  ████████████████████                                                 0 →  400
  P4                      █████                                          400 →  500
  P2                           ██████████                                500 →  700
  P3                                     ██████████████████████████████  700 → 1300
      └────────────────────── total time: 1300 ───────────────────────┘
      average turnaround (arrival → completion): 575
```

Both finish at 1300 — the same total work has to be done either way. But the average wait drops from
725 to 575, purely from the order of decisions. **That difference is what a scheduling algorithm
is.** The preemptive algorithms go further and interrupt a running process mid-execution to do even
better.

The real output looks like this — one record per process, timestamped by the kernel:

```
[Project1] 6765 1588328509.670561208 1588328511.872358281
           ^pid ^started              ^finished
```

---

## The four algorithms

| Algorithm | Preemptive | Rule |
|---|---|---|
| **FIFO** — First-In First-Out | No | Run processes in order of arrival. Whoever arrived first runs to completion. |
| **SJF** — Shortest Job First | No | Among the processes that have arrived, run the one with the least total work. Once started, it runs to completion. |
| **PSJF** — Preemptive Shortest Job First | Yes | The same rule, re-evaluated continuously against *remaining* work. If a shorter process arrives, it takes the CPU immediately. |
| **RR** — Round-Robin | Yes | Give each process a fixed 500-unit slice in rotation. When a slice expires and someone else is waiting, the CPU moves on. |

FIFO and SJF share their selection logic with their preemptive counterparts; what makes them
non-preemptive is a single rule applied first — *if something is already running, leave it alone*.
PSJF gets preemption almost for free: because remaining work shrinks continuously, "shortest job" is
simply re-asked at every step.

---

## Program structure

Five components, each with one responsibility:

| Component | Responsibility |
|---|---|
| [main.c](main.c) | Reads the process list from standard input, prepares the synchronization primitives, and hands control to the requested algorithm. |
| [scheduling.c](scheduling.c) | **The scheduler.** Owns the clock and the main loop, decides which process should run at each step, and orders the switches. Contains the logic for all four algorithms. |
| [process.c](process.c) | **The mechanism layer.** Everything that touches the operating system: creating processes, pinning them to CPU cores, pausing and resuming them, and defining what one unit of work means. The scheduler expresses *intent*; this module knows *how*. |
| [queue.c](queue.c) | A fixed-capacity circular queue, used only by Round-Robin to hold processes waiting for their next turn. |
| [kernel_files/](kernel_files/) | **The kernel side.** The two custom system calls, plus the kernel source files that must be patched to register them. |

```
                 stdin (process list)
                           │
                           ▼
  ┌─────────────────────────────────────────────┐
  │  main         input parsing, setup          │
  ├─────────────────────────────────────────────┤
  │  scheduling   POLICY: who should run now?   │ ◄── queue (Round-Robin only)
  ├─────────────────────────────────────────────┤
  │  process      MECHANISM: make it so         │
  └─────────────────────────────────────────────┘
                           │  system calls
  ═════════════════════════▼═══════════════════════  user space / kernel space
  ┌─────────────────────────────────────────────┐
  │  Linux kernel  (+ 2 custom system calls)    │
  └─────────────────────────────────────────────┘
                           │
                           ▼
                  kernel log → timing results
```

The split between `scheduling.c` and `process.c` is the main structural decision. The algorithm logic
is written purely in terms of "run this one, pause that one" and contains no operating-system calls at
all, so a new algorithm can be added without touching any OS-level code — and the OS-level tricks
below can be changed without touching any algorithm.

---

## How it works

### The central problem

To schedule real processes, the program needs three abilities that an ordinary application does not
normally have:

1. **Pause a process** that should not currently be running.
2. **Resume it** later, exactly where it left off.
3. **Guarantee that nothing else runs in the meantime** — otherwise Linux's own scheduler, not this
   one, is deciding the outcome.

The solution uses two features of the Linux scheduler, neither designed for this purpose. Two terms
are needed first:

| Term | Meaning |
|---|---|
| **`SCHED_OTHER`** | Linux's *normal* scheduling class, where ordinary programs live. Processes in it share the CPU fairly. |
| **`SCHED_IDLE`** | Linux's *lowest-priority* class, meant for background work. A process here receives CPU time only when nothing in the normal class wants it. |
| **CPU affinity** | A restriction on which CPU cores a given process is allowed to run on. |

**Pausing and resuming, via scheduling class.** Rather than stopping processes with signals, the
scheduler *demotes* a process to `SCHED_IDLE`, where it is starved of CPU time by anything in the
normal class. To resume it, the scheduler promotes it back to `SCHED_OTHER`. The process is never
suspended in the OS sense — it simply stops being chosen. This avoids the delay and bookkeeping of
signal-based suspension, and the transition takes effect immediately.

**Removing interference, via CPU affinity.** The scheduler pins itself to one CPU core and confines
every workload process to a second core. Because the whole workload shares a single core and all but
one of them sit in `SCHED_IDLE`, the kernel has exactly one candidate to choose from at any instant —
the one this scheduler selected. Meanwhile the scheduler's own CPU consumption stays off the core
being measured.

```
        CPU 0                                CPU 1
  ┌─────────────────────┐            ┌────────────────────────────────┐
  │                     │            │    P1    SCHED_IDLE  (paused)  │
  │   The scheduler     │  controls  │    P2    SCHED_OTHER (RUNNING) │
  │   (parent process)  │ ─────────► │    P3    SCHED_IDLE  (paused)  │
  │                     │            │    P4    SCHED_IDLE  (paused)  │
  └─────────────────────┘            └────────────────────────────────┘
   scheduler's own core                     workload core
```

### The main loop

The scheduler advances a clock one **time unit** at a time — a fixed amount of CPU burning, measured
at ≈ 2.4 ms, which is the granularity at which all decisions are made and all input is expressed. On
every step it:

1. **Admits new arrivals** — any process whose arrival time has come is created and immediately
   paused, so it exists but consumes nothing.
2. **Applies the policy** — asks the active algorithm which process should hold the CPU now.
3. **Switches if the answer changed** — pauses the outgoing process, resumes the incoming one.
4. **Charges one unit** of work to whichever process is running, and cleans it up once its work is
   done.

Newly created processes are additionally held at a starting line by a *semaphore* — a synchronization
primitive that blocks a process until it is explicitly released. Without it, a process could consume
CPU time in the gap between being created and being dispatched for the first time, corrupting the
measurement before the scheduler had made a single decision.

---

## Extending the kernel

The experiment needed two things the standard system could not provide well:

- **Timestamps that do not disturb what they measure.** Reading the clock from user space, and
  printing results through the C library, costs time inside the very interval being measured.
- **An output channel separate from the program's own output.** The processes print to standard
  output; the measurements needed somewhere else to go.

Both were solved by adding two system calls — the interface through which a user program requests a
service from the kernel. One reads the kernel's high-resolution real-time clock; the other writes a
timing record into the kernel log, readable afterwards with `dmesg`. Each process records a timestamp
before and after its work and reports the pair through these calls, so measurement happens inside the
kernel and never passes through the application's I/O path.

Adding a system call to Linux is not a change in one place. It required:

1. Writing the two handlers as new kernel source ([kernel_files/my_syscall.c](kernel_files/my_syscall.c)).
2. Registering them in the architecture's system call table, which maps call numbers to handlers —
   taking the first two unused numbers, since reusing an occupied one silently breaks an existing
   system call.
3. Declaring them in the kernel's public header so the rest of the kernel can see them.
4. **Recompiling the entire kernel and rebooting into it.**

One detail worth noting: the kernel function most tutorials use to read the clock had been removed in
Linux 5.6, so the implementation had to be written against the current timekeeping interface instead.
Reading the kernel source is part of the work.

---

## Build and run

### Kernel side (one time)

In a Linux 5.6.7 source tree: copy the three files from [kernel_files/](kernel_files/) into their
corresponding locations, add the new source file to the kernel build, then rebuild and install:

```bash
make -j$(nproc) && sudo make modules_install && sudo make install && sudo reboot
```

### User side

```bash
make
sudo ./main < input.txt    # root is required to change another process's scheduling class and CPU
dmesg | tail               # timing results appear in the kernel log
```

### Input format

```
FIFO        # algorithm: FIFO | RR | SJF | PSJF
4           # number of processes
P1 0 400    # name, arrival time, amount of work (both in time units)
P2 100 200
P3 200 600
P4 300 100
```

The program prints one line per process as it is created (name and process id); the timing
measurements go to the kernel log. Captured results for every test case are in [output/](output/).

---

## Results

Full analysis: **[report.md](report.md)**.

- **One time unit measures ≈ 2.4 ms**, established by a calibration run before anything else was
  interpreted.
- **Every run finishes later than theory predicts** — all 20 cases, all four algorithms. The gap is
  the cost of context switching and of running the scheduler itself, and it *accumulates*: in one
  7-process run the first process finished 5,170 units late and the last finished 14,957 units late,
  because each inherits all the lateness of everything scheduled before it.
- **Round-Robin is the most expensive**, drifting roughly three times further from theory than the
  others. Its fixed slice forces context switches on a schedule set by the clock rather than by the
  workload, and switches are exactly what cost time here.
- **Preemption leaves a visible fingerprint in the raw data.** Under the non-preemptive algorithms,
  processes' execution windows never overlap; under the preemptive ones they overlap heavily — in one
  Round-Robin case, four processes were in flight simultaneously across a 128-second run. That
  overlap is the trade-off the algorithms exist to make: Round-Robin's processes all begin making
  progress early, at the cost of each one taking longer individually.

---

## Environment

| | |
|---|---|
| Virtual machine | VMware Workstation 15 Player |
| OS | Ubuntu 20.04 |
| Kernel | Linux **5.6.7**, patched and rebuilt from source |
| CPUs | 2 cores — required, since the design separates scheduler and workload by core |

This program **cannot run on a standard system**: the two custom system calls do not exist in an
unmodified kernel. It also needs root privileges, because changing another process's scheduling class
or CPU assignment is a privileged operation.

---

## Repository layout

```
.
├── main.c                  Input parsing and setup
├── scheduling.c/.h         The scheduler: clock, main loop, and all four algorithms
├── process.c/.h            OS mechanism: process creation, core pinning, pause/resume
├── queue.c/.h              Circular queue (Round-Robin's waiting line)
├── Makefile
├── kernel_files/           The two custom system calls and the kernel files they patch
├── output/                 20 experiment runs plus a calibration run
├── report.md               Detailed results and analysis
├── report.pdf              Original course report, as submitted
└── demo.mp4                Screen recording of a live run
```

---

## Limitations

Kept as submitted; these are what I would change today.

- **It no longer compiles on modern toolchains.** A shared variable is declared in a header in a way
  that older compilers tolerated by default and current ones reject. A one-line fix, but worth
  stating rather than letting a reader discover it.
- **Failures are silent.** The custom system calls' return values are never checked, so on an
  unmodified kernel the program runs to completion and simply produces no timing data, instead of
  reporting the problem.
- **The time unit is machine-specific.** It is defined by a busy loop calibrated on one machine;
  different hardware, or a more aggressive compiler, changes what a "time unit" means.
- **The test inputs were not committed** — only the captured outputs, so the experiments can be read
  but not directly reproduced.
- **No automated tests.** Correctness was verified by inspecting execution traces by hand.

---

## What I took away

Building a scheduler on top of a real kernel rather than a simulated one turned three textbook
abstractions into concrete engineering problems. *Preemption* became a question of which kernel
facility can actually stop a process without disturbing it. *Fairness* became a measurable cost,
paid in context switches. And *measurement itself* became part of the problem — the observation has
to happen inside the kernel, or it changes the thing being observed.

The result is the gap between theory and reality, made visible: the algorithms behave exactly as the
textbook says, and are consistently slower than the textbook implies.
