# Lab 06: Shared-Memory Programming with OpenMP I

**Name:** Hameed Ur Rehman &nbsp;|&nbsp; **Roll No:** 232520 &nbsp;|&nbsp; **Class:** BSCS VII C &nbsp;|&nbsp; **Course:** CS426L Introduction to Parallel & Distributed Computing

**Test machine:** 8 logical cores (`nproc` = 8), macOS host.
**Toolchain:** GCC 14.4.0 with OpenMP (libgomp), running in a Linux container (`gcc:14` Docker image) instead of WSL, the same setup as Labs 04 and 05.
Raw console output from every run is in [output.txt](output.txt). All numbers in this report are copied from that file.

## Project Structure

| File | Task |
|---|---|
| [hello_omp.c](hello_omp.c) | Task 1: Parallel region that prints each thread's ID and the team size |
| [vecadd.c](vecadd.c) | Task 2: Vector addition with `#pragma omp parallel for` and timing |
| [scoping_bug.c](scoping_bug.c) | Task 3: Missing-`private` race condition and two fixes |
| [omp_sections.c](omp_sections.c) | Task 4: Independent operations with `#pragma omp sections` |
| [run_all.sh](run_all.sh) | Builds and runs every task in order and prints all results |

### How to Run

On WSL / Linux:

```bash
cd LAB-06
bash run_all.sh | tee output.txt
```

On macOS (Docker Desktop running):

```bash
cd LAB-06
docker run --rm -v "$PWD":/lab gcc:14 bash /lab/run_all.sh | tee output.txt
```

The experiments that the task sheet describes as temporary source edits are compile-time switches here, so the submitted source is always the correct version and every experiment can be repeated:

| Switch | Experiment |
|---|---|
| `-DCLAUSE_THREADS=3` ([hello_omp.c](hello_omp.c)) | Task 1: add a `num_threads(3)` clause to test precedence against `OMP_NUM_THREADS` |
| `-DDROP_N_FROM_SHARED` ([vecadd.c](vecadd.c)) | Task 2: remove `n` from `shared(...)` so `default(none)` rejects the build |
| `-DFIX_PRIVATE` ([scoping_bug.c](scoping_bug.c)) | Task 3 fix 1: add `private(tmp)` |
| `-DFIX_LOCAL` ([scoping_bug.c](scoping_bug.c)) | Task 3 fix 2: declare `tmp` inside the loop |
| `-DDEFAULT_NONE` ([scoping_bug.c](scoping_bug.c)) | Task 3: `default(none)` with `tmp` unscoped — build must fail |

---

## Task 1: Parallel Region and Thread IDs

Each thread in the team prints its own ID (`omp_get_thread_num()`) and the team size (`omp_get_num_threads()`). The program also prints `omp_get_num_threads()` before and after the region.

### Team size and print order

| OMP_NUM_THREADS | Team size reported | ID order, run 1 | ID order, run 2 | Same order? |
|---|---|---|---|---|
| 1 | 1 | 0 | 0 | Y |
| 2 | 2 | 0, 1 | 0, 1 | Y |
| 4 | 4 | 2, 0, 1, 3 | 1, 3, 0, 2 | **N** |
| 8 | 8 | 0, 7, 4, 2, 1, 3, 5, 6 | 0, 7, 4, 2, 6, 3, 5, 1 | **N** |

**Number of logical cores (nproc): 8**
**Team size with `-DCLAUSE_THREADS=3` and `OMP_NUM_THREADS=8`: 3**

### Discussion questions

**1. Were the IDs printed in the same order in every run? Why no ordering guarantee?**
No. With 4 and 8 threads the order changed between runs. The threads run concurrently and the OS schedules them independently, so whichever thread reaches its `printf` first prints first. The OpenMP standard deliberately makes no ordering promise because the whole point of a parallel region is that the threads run at the same time, not in a fixed sequence. To print strictly in ID order you would have to serialise the output — for example `#pragma omp ordered` inside an ordered loop, or an ordered loop with `#pragma omp critical`, or simply collect each thread's text into an array and print it in order after the region.

**2. What did `omp_get_num_threads()` return before and after the region, and why different inside?**
Before and after the region it returned **1**. Outside any parallel region only the single initial (master) thread is running, so the "team" has one member. Inside the region it returned the actual team size (2, 4, 8, …) because there the function reports the number of threads in the team currently executing that region.

**3. Which setting won in the `-DCLAUSE_THREADS=3` experiment?**
The **`num_threads` clause won**: the team size was 3 even though `OMP_NUM_THREADS=8`. The order of precedence for the team size, highest first, is:
`num_threads(...)` clause → `omp_set_num_threads()` call → `OMP_NUM_THREADS` environment variable → the implementation default. A higher-precedence setting, if present, overrides all the ones below it.

**4. What happens when `OMP_NUM_THREADS` is larger than nproc?**
That many threads are still created — OpenMP honours the request. But with more threads than cores they no longer run truly in parallel; the OS time-slices them onto the available cores. Performance can suffer from the extra context switches and from oversubscription of the caches, so going past `nproc` rarely helps and often hurts. In Task 4, 8 threads were no faster than 3.

---

## Task 2: Vector Addition with `parallel for`

`c[i] = a[i] + b[i]` for N = 5,000,000, repeated 20 passes per timed run, with
`#pragma omp parallel for default(none) shared(a, b, c, n) private(i)`.

### Timing and speedup (N = 5,000,000, 20 passes per run)

| Threads | Run 1 (ms) | Run 2 (ms) | Run 3 (ms) | Average (ms) | Speedup | Correct? |
|---|---|---|---|---|---|---|
| 1 | 116.65 | 125.84 | 165.42 | 135.97 | 1.00 | Y |
| 2 | 87.53 | 89.57 | 87.82 | 88.31 | 1.54 | Y |
| 4 | 72.60 | 79.38 | 76.04 | 76.01 | 1.79 | Y |
| 8 | 80.09 | 85.72 | 87.31 | 84.37 | 1.61 | Y |

**Compiler error message when `n` was removed from `shared(...)`:**

```
vecadd.c: In function 'vec_add':
vecadd.c:32:13: error: 'n' not specified in enclosing 'parallel'
   32 |     #pragma omp parallel for default(none) shared(a, b, c) private(i)
      |             ^~~
```

### Discussion questions

**1. Why doesn't the loop variable `i` appear in a data-sharing clause?**
The iteration variable of a `parallel for` is made **private automatically** by the standard — each thread owns its own `i` holding the chunk of iterations it runs. (It is listed in `private(i)` here only to be explicit; it is not required.) A scratch variable declared outside the loop but written inside it is *not* automatic: it would default to shared, so you must put it in a `private(...)` clause (or declare it inside the loop body) or every thread would stamp on the same copy.

**2. What did `default(none)` force you to do, and why is it useful?**
`default(none)` removes the implicit default, so the compiler refuses to guess any variable's sharing — every variable used in the region must be named in `shared`, `private`, etc. When `n` was dropped the build failed with *"'n' not specified in enclosing 'parallel'"* instead of compiling silently. On a large program this turns an easily-missed scoping mistake into a compile error you cannot ignore.

**3. Describe the speedup. Why does vector addition scale poorly?**
Speedup rose to about 1.8× at 4 threads and then fell back to 1.6× at 8. Vector addition is **memory-bandwidth bound**: each element does one add but needs three memory transfers (read `a`, read `b`, write `c`). A few threads are enough to saturate the memory bus, so adding more threads gives them no extra bandwidth to work with — they just contend for the same memory, which is why 8 threads were slower than 4.

**4. Why is the result correct without any lock or atomic?**
Because the iterations are independent: iteration `i` reads `a[i]`, `b[i]` and writes only `c[i]`. No two threads ever touch the same element, so there is no shared write to protect. The only synchronisation needed is the **implicit barrier at the end of the `parallel for`**, which guarantees every element is written before the program reads the array back.

**5. How are the iterations divided by default?**
With the default `static` schedule the N iterations are split into equal contiguous chunks, one per thread (thread 0 gets the first N/t, and so on). For this loop every iteration costs the same, so the division is balanced and the exact schedule makes little difference; it would matter only if some iterations were much heavier than others.

---

## Task 3: A Data-Scoping Bug (Missing `private`) and Its Fix

Each iteration does `tmp = in[i] * 3; out[i] = tmp + 7;`. In the buggy build `tmp` is **shared** by the whole team, so one thread overwrites it between another thread's two statements. The program counts how many `out[i]` came out wrong. Built with `-O0` throughout.

### Wrong elements per run (4 threads)

| Run | Buggy version | FIX_PRIVATE | FIX_LOCAL |
|---|---|---|---|
| 1 | 748 | 0 | 0 |
| 2 | 631 | 0 | 0 |
| 3 | 37 | 0 | 0 |
| 4 | 13 | 0 | 0 |
| 5 | 19 | 0 | 0 |
| 6 | 525 | 0 | 0 |
| 7 | 133 | 0 | 0 |
| 8 | 4245 | 0 | 0 |
| 9 | 2807 | 0 | 0 |
| 10 | 2942 | 0 | 0 |

**Buggy version with `OMP_NUM_THREADS=1`, wrong elements: 0**
**nproc: 8**

**Compiler error message for `-DDEFAULT_NONE`:**

```
scoping_bug.c: In function 'main':
scoping_bug.c:54:13: error: 'tmp' not specified in enclosing 'parallel'
   54 |         tmp = in[i] * 3;
      |         ~~~~^~~~~~~~~~~
scoping_bug.c:52:13: note: enclosing 'parallel'
```

### Discussion questions

**1. Did the buggy version give the same number of wrong elements every run? What with one thread?**
No — the count jumped around wildly (from 13 to 4245) across the ten runs, and with **one thread it was always 0**. That is the signature of a race condition: the result depends on the exact interleaving the scheduler happens to pick, which varies run to run. With a single thread there is no other thread to overwrite `tmp`, so the bug cannot appear. This is also why a correct-looking run proves nothing — run 4 produced only 13 wrong elements, but the code was still broken.

**2. A two-thread interleaving that makes `out[i]` wrong.**
Thread 0 works on index `i`, thread 1 on index `j`, sharing the one `tmp`:

```
T0: tmp = in[i] * 3;      // tmp now holds 3*in[i]
T1: tmp = in[j] * 3;      // T1 overwrites tmp with 3*in[j]
T0: out[i] = tmp + 7;     // T0 reads the WRONG tmp -> out[i] = 3*in[j] + 7
T1: out[j] = tmp + 7;     // out[j] happens to be correct this time
```

`out[i]` ends up as `3*in[j] + 7` instead of `3*in[i] + 7`.

**3. Compare `private`, `firstprivate` and `lastprivate`.**
A plain **`private`** copy starts **uninitialised** — it does *not* inherit the original value. **`firstprivate`** starts each thread's copy initialised to the value the variable had before the region. **`lastprivate`** copies the value from the logically last iteration back out to the original after the region. If the copy had to start with the original's value you would use **`firstprivate`**.

**4. What does `default(none)` do, and why prefer a compile error?**
It forces every variable to be explicitly scoped; leaving `tmp` out made the build fail. A compile error is far better than a wrong result at run time because it is caught immediately and deterministically — whereas the race can hide for thousands of correct-looking runs (as run 4 shows) and only surface in production.

**5. Why is the buggy build compiled with `-O0`? Would a correct-looking `-O2` run mean it was fixed?**
`-O0` keeps `tmp` in memory and keeps the timing loose, which makes the race easy to observe. At `-O2` the optimiser may keep `tmp` in a register or reorder the code so the window closes and the wrong answers rarely show — but the bug is still there. A correct-looking `-O2` run would **not** mean it was fixed; only scoping `tmp` correctly fixes it.

---

## Task 4: Independent Operations with `sections`

Three independent reductions run concurrently, one per `#pragma omp section`:
A = Σ√i, B = Σ sin·cos, C = Σ log(i+1), over 120,000,000 iterations each. The program times each operation serially first, then inside a `sections` construct, and checks the results match.

### Sections with different team sizes

| Threads | Wall time (s) | Speedup | Thread of A / B / C | Results match? |
|---|---|---|---|---|
| 1 | 3.080 | 1.04 | 0 / 0 / 0 | Y |
| 2 | 2.342 | 1.36 | 0 / 1 / 0 | Y |
| 3 | 2.290 | 1.40 | 1 / 0 / 2 | Y |
| 4 | 2.395 | 1.33 | 3 / 2 / 0 | Y |
| 8 | 2.368 | 1.35 | 6 / 5 / 0 | Y |

**Serial times (s): A ≈ 0.32, B ≈ 2.26, C ≈ 0.61, total ≈ 3.19**

Three-thread assignment over three runs: A→0, B→1, C→2 every time (on this build/runtime), but see Q3.

### Discussion questions

**1. Which operation took longest? What limits the speedup?**
**B** was by far the longest (≈ 2.26 s against A ≈ 0.32 s and C ≈ 0.61 s). With 3 threads the wall time was 2.29 s — essentially equal to B alone, and much less than the 3.19 s serial total. The speedup is limited to about **total / longest = 3.19 / 2.26 ≈ 1.4×** because each section is one indivisible unit of work: once A and C finish early, their threads sit idle waiting for B at the implicit barrier. `sections` can only run the three pieces side by side; it cannot split the longest one.

**2. Why did 4 and 8 threads not improve on 3?**
There are only **three sections**, so at most three threads can ever be busy. With 4 or 8 threads the extra threads are handed no section — they go straight to the implicit barrier and wait. The run time stays pinned to section B, so 4 and 8 threads were no faster than 3.

**3. Was the same thread assigned to the same section every run? Does the standard guarantee it?**
In these three 3-thread runs the assignment happened to be stable (A→0, B→1, C→2), but the standard gives **no guarantee** at all about which thread runs which section — at 4 threads A was run by thread 3, at 8 by thread 6. The mapping of sections to threads is entirely implementation- and schedule-dependent, so correct code must never rely on it.

**4. Why must sections be independent?**
Because they run concurrently with no synchronisation between them. If A and B both wrote to the same variable, their writes could interleave into a race exactly like Task 3 — one section's update lost or read half-finished by the other — giving a nondeterministic wrong result. Each section writing only its own output (`a_par`, `b_par`, `c_par` here) is what makes running them in parallel safe.

**5. When choose `sections` over `parallel for`, and what does the implicit barrier mean?**
Use `sections` for a **small, fixed number of different tasks** that can run at once (here three distinct reductions); use `parallel for` when you have **many identical iterations** of one loop. The implicit barrier at the end of the `sections` construct means no thread proceeds until every section has finished, so any code after the construct can safely assume all three results are ready — which is why the `results_match` check always passed.
