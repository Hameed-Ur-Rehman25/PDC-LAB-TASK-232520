# Lab 07: OpenMP II — Scheduling & Reduction

**Name:** Hameed Ur Rehman &nbsp;|&nbsp; **Roll No:** 232520 &nbsp;|&nbsp; **Class:** BSCS VII C &nbsp;|&nbsp; **Course:** CS426L Introduction to Parallel & Distributed Computing

**Test machine:** 8 logical cores (`nproc` = 8), cache-line size 64 bytes (`getconf LEVEL1_DCACHE_LINESIZE`), macOS host.
**Toolchain:** GCC 14.4.0 with OpenMP (libgomp), running in a Linux container (`gcc:14` Docker image) instead of WSL, the same setup as Labs 04 to 06.
Raw console output from every run is in [output.txt](output.txt). All numbers in this report are copied from that file.

## Project Structure

| File | Task |
|---|---|
| [reduction.c](reduction.c) | Task 1: parallel sum and max — `reduction` clause vs critical sections |
| [schedule_bench.c](schedule_bench.c) | Tasks 2 & 4: static / dynamic / guided scheduling on uneven and (`-DUNIFORM`) uniform workloads |
| [false_sharing.c](false_sharing.c) | Task 3: adjacent counters, padding and local accumulation |
| [run_all.sh](run_all.sh) | Builds and runs every task in order and prints all results |

### How to Run

On WSL / Linux:

```bash
cd LAB-07
bash run_all.sh | tee output.txt
```

On macOS (Docker Desktop running):

```bash
cd LAB-07
docker run --rm -v "$PWD":/lab gcc:14 bash /lab/run_all.sh | tee output.txt
```

Task 4 is the same `schedule_bench.c` rebuilt with `-DUNIFORM`, which changes each iteration's cost from `i` steps to a constant `N/2` steps; `run_all.sh` does this automatically.

---

## Task 1: Parallel Sum and Max — `reduction` vs `critical`

Sum and maximum of 10,000,000 integers computed five ways: `serial` (reference), `racy` (unprotected `s += data[i]`), `critical-each` (every update inside `#pragma omp critical`), `critical-partial` (hand-written private partials + one critical merge per thread), and `reduction` (`reduction(+:s) reduction(max:m)`). Each version is averaged over 3 repetitions.

### Variants with 4 threads

| Variant | Sum correct? | Max correct? | Average time (ms) |
|---|---|---|---|
| serial | Y | Y | 4.00 |
| racy | **N** | Y | 2.16 |
| critical-each | Y | Y | 681.68 |
| critical-partial | Y | Y | 2.21 |
| reduction | Y | Y | 1.83 |

The reference sum is **500009931972** and the reference max is **100002**. The racy run produced sum **125002518436** (≈ ¼ of the correct value with 4 threads) while its max stayed correct.

### Scaling (average time in ms)

| Threads | critical-each | critical-partial | reduction | Reduction speedup |
|---|---|---|---|---|
| 1 | 171.70 | 7.79 | 7.70 | 1.00 |
| 2 | 337.89 | 4.67 | 4.68 | 1.64 |
| 4 | 650.09 | 2.07 | 2.11 | 3.65 |
| 8 | 1028.39 | 4.85 | 1.50 | 5.13 |

`critical-each` gets **slower** with more threads (171 → 1028 ms), while `reduction` scales up to ≈ 5× at 8 threads. The racy sum across the three 4-thread runs was **125002518436**, **125002447550** and **125002504513** — close but not identical.

### Discussion questions

**1. Why does the racy version give a wrong sum, and was the wrong value the same every run? Why is the maximum often still correct?**
`s += data[i]` is read-modify-write on one shared `s`. Threads read the same old value, add their element and write back, so updates stamp on each other and are lost — with `t` threads roughly `1/t` of the work survives (≈ ¼ at 4 threads, ≈ ⅛ at 8, which matches the numbers above). The wrong value was **not** identical across runs (125002518436 vs 125002447550 vs 125002504513): the exact loss depends on the scheduler's interleaving, the signature of a race. The **maximum** usually stays correct because `if (data[i] > m) m = data[i];` only ever moves `m` upward; a lost write just means some thread didn't raise `m`, but another thread that saw the true maximum still raised it, and once `m` holds the largest value no later write can lower it. (It could in principle be wrong if the thread holding the true max were interrupted between the compare and the store, but that window is tiny, so it is almost always right.)

**2. Why is `critical-each` so much slower than the serial loop, and why no faster with more threads?**
It enters a critical section **10 million times**, once per element. A critical section lets only one thread run at a time, so the whole loop is effectively serialised — but now with the full cost of locking and unlocking on every iteration *plus* cache-line contention on the lock. That is why it is ~170× slower than the serial loop at one thread and gets **worse** with more threads (171 → 1028 ms): extra threads add no parallel work (they are all waiting for the lock) and only increase the contention for it.

**3. How does the `reduction` clause work, and what are the identity values?**
Each thread is given a **private copy** of the reduction variable, initialised to the operator's **identity value** — `0` for `+` and `INT_MIN` (the smallest `int`) for `max`. Every thread accumulates only into its own private copy during the loop, with no synchronisation, so there is no contention. At the **end of the loop** the runtime combines all the private copies into the original shared variable using the same operator. This is exactly the private-partial-then-merge pattern, done by the compiler.

**4. Compare `critical-partial` with `reduction`.**
They do the **same thing**: both give every thread a private partial result, run the whole loop with no per-element locking, and merge once at the end (one critical section per thread vs the runtime's combine step). Their times are nearly identical (2.21 vs 1.83 ms at 4 threads). The clause is preferable because it is one line, cannot be got wrong (no forgotten critical section, no wrong identity value), expresses intent clearly, and lets the runtime pick an efficient tree-combine instead of a serial chain of criticals.

**5. Why could a floating-point reduction differ slightly between runs or thread counts?**
Floating-point addition is **not associative**: `(a+b)+c` can round differently from `a+(b+c)`. A reduction adds the elements in a different grouping depending on how many threads there are and how their partials are combined, so the rounding error accumulates differently and the last few digits can change. Integers are exact and associative, so every correct version here gives bit-for-bit the same answer, which is why the lab uses them for the correctness check.

---

## Task 2: Benchmarking Static, Dynamic and Guided Scheduling

A deliberately **uneven** (triangular) workload: iteration `i` performs `i` steps of arithmetic, so late iterations cost far more than early ones (N = 20,000). One executable benchmarks seven policies via `schedule(runtime)` + `omp_set_schedule()`. Each thread times its own busy period; **imbalance** = busy_max / busy_min (1.0 is perfect).

### Uneven workload with 4 threads (best of 3 runs)

| Policy | Wall (ms) | Busy max (ms) | Busy min (ms) | Imbalance | Correct? |
|---|---|---|---|---|---|
| static | 123.5 | 123.4 | 18.7 | 6.61 | Y |
| static,1 | 68.6 | 68.6 | 67.0 | 1.02 | Y |
| static,64 | 70.5 | 70.5 | 68.7 | 1.03 | Y |
| dynamic,1 | 66.5 | 66.5 | 66.4 | 1.00 | Y |
| dynamic,64 | 67.1 | 67.1 | 66.5 | 1.01 | Y |
| guided,1 | 68.2 | 68.2 | 68.2 | 1.00 | Y |
| guided,64 | 67.3 | 67.3 | 66.0 | 1.02 | Y |

**Number of logical cores (nproc): 8** &nbsp;|&nbsp; **Serial time (≈): 281.8 ms** &nbsp;|&nbsp; **Fastest policy at 2 / 4 / 8 threads: `dynamic,1` (128.8) / `dynamic,1` (66.5) / `dynamic,64` (44.8)**

The imbalance of plain `static` grew sharply with thread count: **3.03** at 2 threads, **6.61** at 4, **12.61** at 8 — while every balanced policy stayed near 1.0.

### Discussion questions

**1. Why does plain `static` show a large imbalance, and which thread is busiest?**
`static` with no chunk gives each thread one contiguous block: thread 0 gets iterations 0..N/4, thread 3 gets 3N/4..N. Because cost grows with `i`, the **last thread (thread 3)** gets the heaviest iterations and the first thread the lightest. At 4 threads thread 3 was busy 123.4 ms while the lightest thread was busy only 18.7 ms (imbalance 6.61). The wall time is set by the slowest thread, so the others sit idle after finishing early.

**2. Why do `static,1`, dynamic and guided balance much better, and what does each pay?**
`static,1` deals iterations out round-robin one at a time, so every thread gets an even mix of cheap and expensive iterations — balanced in advance, paid for by scattered (less cache-friendly) access but **no runtime scheduling cost**. `dynamic` lets a free thread grab the next chunk, so a thread that drew heavy work simply claims fewer chunks — it pays **one synchronised claim per chunk** (worst at chunk 1). `guided` is dynamic with chunks that start large and shrink, so it balances well with **far fewer claims** than `dynamic,1`. All three brought imbalance to ≈ 1.0 and nearly halved the wall time versus plain `static`.

**3. How did 2 and 8 threads change things, and what does busy time measure past nproc?**
The best policies stayed the dynamic/guided family at every count, and plain `static`'s imbalance *rose* with more threads (3.03 → 6.61 → 12.61) because the last block becomes a larger fraction of the total cost. With 8 threads on 8 logical cores the machine is fully used; if threads **exceed cores**, a thread's busy time no longer measures pure computation — it includes time the OS spent running *other* threads while this one was descheduled (time-slicing), so busy times inflate and become noisier.

**4. Effect of chunk size — dynamic,1 vs dynamic,64?**
Both balanced well here (66.5 vs 67.1 ms at 4 threads). Chunk 1 balances best (finest granularity) but makes the most scheduling claims; chunk 64 makes 64× fewer claims (less overhead, better locality) at the cost of slightly coarser balance. The trade-off is **balance quality vs scheduling/locality overhead**; on this workload the work per iteration is large enough that the overhead of chunk 1 is negligible, so they tie.

**5. Why does `guided` start large and shrink, and when is that an advantage?**
Large early chunks hand out most of the work cheaply (few claims) while plenty remains; small late chunks give fine-grained balance exactly when it matters, near the end where a bad final chunk would leave one thread running alone. This beats `dynamic,1` when there are **many iterations and claiming a chunk is not free** — guided gets nearly the same balance with a fraction of the synchronisation.

---

## Task 3: False Sharing and Padding

Each thread increments its own counter 50,000,000 times. `adjacent`: counters are consecutive `long`s (8 bytes apart → 8 share one 64-byte line). `padded`: each counter is a 64-byte cache-line-aligned struct. `local-accumulate`: counts in a private local and writes the counter once. **Cache-line size: 64 bytes.**

### Three runs with 4 threads

| Run | adjacent (ms) | padded (ms) | local-accumulate (ms) | Correct? |
|---|---|---|---|---|
| 1 | 178.13 | 32.25 | 20.69 | Y |
| 2 | 158.84 | 28.60 | 20.40 | Y |
| 3 | 164.47 | 30.09 | 19.41 | Y |

### Effect of the number of threads

| Threads | adjacent (ms) | padded (ms) | Slowdown adjacent / padded |
|---|---|---|---|
| 1 | 124.83 | 19.78 | 6.31 |
| 2 | 129.53 | 20.73 | 6.25 |
| 4 | 157.71 | 34.03 | 4.63 |
| 8 | 367.07 | 170.21 | 2.16 |

**Cache-line size (bytes): 64** &nbsp;|&nbsp; **Number of logical cores (nproc): 8**

Note: the `adjacent` variant is always timed **first** inside `run()`, so its single-thread figure also carries some container cold-start cost; the physically meaningful comparison is that `adjacent` stayed 4–6× slower than `padded` whenever several cores ran in parallel, and both ballooned at 8 threads where the machine is fully subscribed.

### Discussion questions

**1. Why is `adjacent` correct but slower, even though no two threads write the same variable?**
Each thread writes only its own `adjacent[id]`, so the result is correct. But the CPU moves memory in **whole 64-byte cache lines**, and eight 8-byte counters sit on one line. When one core writes its counter, the cache-coherence protocol **invalidates that line in every other core**, forcing them to re-fetch it before their own next write. The line ping-pongs between cores on every increment even though the data is logically private — **false sharing**: correct, but the per-write cost is a cross-core transfer instead of an L1 hit.

**2. Why does padding fix it, and what does it cost? Why small effect with 1 thread?**
Padding each counter to a full 64-byte line means no two threads' counters share a line, so one core's write never invalidates another's — each stays hot in that core's L1. The cost is **memory**: 64 bytes per counter instead of 8 (here 8× for mostly padding). With **1 thread** there is no other core to invalidate the line, so there is nothing to fix — the small apparent gap in the table is cold-start noise, not false sharing.

**3. Which version is fastest, and why is local accumulation the best remedy?**
`local-accumulate` is fastest (≈ 20 ms vs 30 ms padded at 4 threads). It keeps the running count in a **register/local** and touches the shared counter only **once** at the very end, so there is no repeated cross-core traffic *and* no wasted padding memory. Padding is still the right tool when a thread genuinely must update shared state **frequently** throughout the loop (so you cannot defer the writes) but each thread's slot can live on its own line.

**4. How did the slowdown change with thread count, and what on a single core?**
The absolute `adjacent` time rose with more active cores (124 → 367 ms) as more cores fight over the shared line; the *ratio* to padded fell toward 8 threads only because `padded` itself slowed under oversubscription (8 threads on 8 cores plus runtime overhead). On a genuinely **single core** there is no cross-core invalidation, so `adjacent` and `padded` would take essentially the same time — false sharing needs real parallel hardware.

**5. How would you recognise false sharing in a real program?**
Symptoms: a parallel loop that **scales badly or slows down** with more threads despite having no locks and no logically shared writes, with high cache-coherence / cache-miss counts. Tools: a profiler such as `perf c2c` (Linux) which pinpoints cache lines bouncing between cores, or hardware counters for coherence misses (e.g. VTune's "contested accesses"). The fix is the same — pad hot per-thread data onto separate lines, or accumulate locally and write once.

---

## Task 4: Scheduling Policy Summary

The same benchmark on a **uniform** workload (`-DUNIFORM`, every iteration costs `N/2` steps) placed beside the uneven results, both at 4 threads. Uniform wall times from the dedicated run; uneven wall times/imbalance carried over from Task 2.

### Summary of the two workloads (4 threads)

| Policy | Uneven: wall (ms) | Uneven: imbalance | Uniform: wall (ms) | Uniform: imbalance | Verdict |
|---|---|---|---|---|---|
| static | 123.5 | 6.61 | 72.8 | 1.02 | Worst on uneven; fine on uniform |
| static,1 | 68.6 | 1.02 | 76.4 | 1.02 | Balances uneven; slowest on uniform |
| static,64 | 70.5 | 1.03 | 69.2 | 1.03 | Good on both |
| dynamic,1 | **66.5** | 1.00 | 70.7 | 1.00 | **Best on uneven**; small overhead on uniform |
| dynamic,64 | 67.1 | 1.01 | **66.8** | 1.01 | **Best on uniform**; near-best on uneven |
| guided,1 | 68.2 | 1.00 | 70.1 | 1.00 | Balanced on both |
| guided,64 | 67.3 | 1.02 | 70.0 | 1.01 | Balanced on both |

### Written summary

On the **uneven** workload the balancing policies were decisively best: `dynamic,1` gave the lowest wall time (66.5 ms) with perfect imbalance (1.00), and the whole dynamic/guided family plus `static,1` clustered near 67–70 ms, while plain `static` was nearly twice as slow (123.5 ms) because its imbalance of 6.61 left one thread doing most of the work. On the **uniform** workload the picture flattened: every policy finished in 66–76 ms with imbalance ≈ 1.0, and `dynamic,64` was marginally fastest (66.8 ms) with cheap `static,64` essentially tied (69.2 ms). The key lesson is that on uniform work the balancing machinery earns nothing — `static,1` was actually the *slowest* uniform policy (76.4 ms) because it pays for scattered access it does not need. My rule of thumb: **use `static` (or a moderate `static,k`) when iterations cost the same, and `dynamic` or `guided` with a moderate chunk when they do not**; if the unevenness is unknown, `guided,k` or `dynamic,64` is the safe default because it balances the uneven case while adding almost nothing to the uniform one.

### Discussion questions

**1. Best policy for each workload, with numbers?**
Uneven: `dynamic,1` at **66.5 ms** (imbalance 1.00) vs plain `static` at 123.5 ms. Uniform: `dynamic,64` at **66.8 ms**, with `static,64` tied at 69.2 ms and all policies within 66–76 ms.

**2. Why can `static` be best for uniform work though worst for uneven?**
When every iteration costs the same, splitting into equal contiguous blocks is already perfectly balanced (uniform imbalance 1.02), and `static` pays **zero scheduling overhead** and has the best cache locality. On uneven work that same fixed split is exactly what causes the imbalance, so it becomes the worst.

**3. Safe default if unevenness is unknown?**
`guided,k` (or `dynamic` with a moderate chunk like 64). It balances the uneven case almost as well as `dynamic,1` while making far fewer scheduling claims, so on uniform work it costs only a hair more than `static` (66.8–70 ms vs 69–73 ms). It protects against the catastrophic `static` imbalance without a large penalty when the work turns out to be even.

**4. What changes if each iteration is extremely cheap (a few ns)?**
Then the **cost of claiming a chunk rivals the work itself**, so fine-grained `dynamic,1`/`guided,1` would spend most of their time in the runtime, not computing. In that regime `static` (or a large chunk) wins: you accept some imbalance to avoid per-chunk synchronisation that would dominate. Use a large chunk size so each claim amortises many iterations.

**5. How do `schedule(runtime)` + `OMP_SCHEDULE` help tune without recompiling?**
`schedule(runtime)` defers the policy choice to execution, taking it from `OMP_SCHEDULE` (or `omp_set_schedule()`). You can then try `OMP_SCHEDULE="dynamic,64"`, `"guided,1"`, `"static"`, etc. on the **same binary** and measure, picking the best for the real workload and hardware without touching or rebuilding the source — exactly how this lab benchmarked seven policies from one executable.
