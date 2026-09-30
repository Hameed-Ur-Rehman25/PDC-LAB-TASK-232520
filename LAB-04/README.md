# Lab 04: Concurrency Issues

**Name:** Hameed Ur Rehman &nbsp;|&nbsp; **Roll No:** 232520 &nbsp;|&nbsp; **Class:** BSCS VII C &nbsp;|&nbsp; **Course:** CS426L Introduction to Parallel & Distributed Computing

**Test machine:** Intel Core i7-1068NG7 @ 2.30 GHz, 8 logical cores (`nproc` = 8), macOS host.
**Toolchain:** GCC 14.4.0 with ThreadSanitizer, running in a Linux container (`gcc:14` Docker image) instead of WSL. Apple clang's ThreadSanitizer crashes on this macOS version, so a Linux GCC environment was used to match the lab setup.
Raw console output from every run is in [output.txt](output.txt). The full Task 2 TSan report is in [tsan_report.txt](tsan_report.txt).

## Project Structure

| File | Task |
|---|---|
| [race.c](race.c) | Task 1: Unsynchronized counter |
| [run_tsan.sh](run_tsan.sh) | Task 2: Builds the unchanged `race.c` with ThreadSanitizer and saves the report to [tsan_report.txt](tsan_report.txt) |
| [race_mutex.c](race_mutex.c) | Task 3: Mutex-protected counter |
| [counter_bench.c](counter_bench.c) | Task 4: Mutex vs. atomic benchmark |
| [run_all.sh](run_all.sh) | Runs every task in order and prints all results |

### How to Run

On WSL / Linux:

```bash
cd LAB-04
bash run_all.sh | tee output.txt
```

On macOS (Docker Desktop running):

```bash
cd LAB-04
docker run --rm -v "$PWD":/lab gcc:14 bash /lab/run_all.sh | tee output.txt
```

Individual build commands used:

```bash
# Task 1
gcc -O0 -pthread race.c -o race
# Task 2
gcc -fsanitize=thread -g -O1 -pthread -DINCREMENTS_PER_THREAD=100000 race.c -o race_tsan
# Task 3
gcc -O0 -pthread race_mutex.c -o race_mutex
gcc -fsanitize=thread -g -O1 -pthread -DINCREMENTS_PER_THREAD=100000 race_mutex.c -o race_mutex_tsan
# Task 4
gcc -O2 -pthread -std=gnu11 -DUSE_MUTEX counter_bench.c -o bench_mutex
gcc -O2 -pthread -std=gnu11 -DUSE_SYNC  counter_bench.c -o bench_sync
gcc -O2 -pthread -std=gnu11 -DUSE_C11   counter_bench.c -o bench_c11
gcc -fsanitize=thread -g -O1 -pthread -std=gnu11 -DUSE_SYNC -DINCREMENTS_PER_THREAD=100000 counter_bench.c -o bench_sync_tsan
```

---

## Task 1: Unsynchronized Counter (Shared-State Hazard)

Four threads each run `counter++` 1,000,000 times on one global `long counter` with no lock and no atomic operation. The program was compiled with `-O0`, so every increment really performs a load, an add and a store on memory.

### Results

| Run | Expected Value | Actual Value | Lost Updates (Expected − Actual) |
|---|---|---|---|
| 1 | 4,000,000 | 1,319,235 | 2,680,765 |
| 2 | 4,000,000 | 1,194,209 | 2,805,791 |
| 3 | 4,000,000 | 1,191,599 | 2,808,401 |
| 4 | 4,000,000 | 1,123,237 | 2,876,763 |
| 5 | 4,000,000 | 1,364,029 | 2,635,971 |
| 6 | 4,000,000 | 1,433,325 | 2,566,675 |
| 7 | 4,000,000 | 1,466,143 | 2,533,857 |
| 8 | 4,000,000 | 1,117,242 | 2,882,758 |
| 9 | 4,000,000 | 1,371,491 | 2,628,509 |
| 10 | 4,000,000 | 1,206,069 | 2,793,931 |

Number of logical cores (`nproc`): **8**

On average, 2,721,342 updates (about 68%) were lost per run. The loss ranged from 63% to 72%.

### Discussion

**1. Did the actual value ever equal the expected value? Were any two runs identical? Why is the result non-deterministic?**
No. None of the 10 runs reached 4,000,000, and no two runs gave the same value (they ranged from 1,117,242 to 1,466,143). The source code and input never changed, but the *interleaving* of the threads did. The OS scheduler decides which thread runs on which core and when it is preempted. Cache timing, interrupts and other programs on the machine also shift the timing slightly on every run. The number of lost updates depends on how often two threads' load/add/store sequences overlap, so it is different each time.

**2. Using the load / add / store breakdown, explain how one increment can be lost.**
`counter++` compiles to three machine steps: load `counter` into a register, add 1 to the register, and store the register back to `counter`. Suppose `counter` is 5. Thread A loads 5. Before A stores, thread B also loads 5. A adds 1 and stores 6. B adds 1 to *its own* copy (also 5) and stores 6. Two increments ran, but `counter` went from 5 to 6: B's store overwrote A's result, so A's update was lost. With 8 cores running 4 threads truly in parallel, this happens millions of times per run.

**3. If INCREMENTS_PER_THREAD were 100, would the race disappear?**
No, it would only become harder to observe. 100 increments take well under a microsecond, while creating a thread takes tens of microseconds. So each thread would usually finish before the next one even starts, and the loops would almost never overlap. The output would nearly always show 400, but the code still contains the same unsynchronized read-modify-write. On a busier machine, or if a thread were delayed, updates could still be lost. A correct output only shows that *this particular run* happened to have a harmless interleaving. It does not show that every possible interleaving is safe. Task 2 demonstrates this directly: the TSan build printed the correct total and still reported the race.

---

## Task 2: Detecting the Race with ThreadSanitizer

The **unchanged** Task 1 program was rebuilt with ThreadSanitizer ([run_tsan.sh](run_tsan.sh)):

```bash
gcc -fsanitize=thread -g -O1 -pthread -DINCREMENTS_PER_THREAD=100000 race.c -o race_tsan
./race_tsan 2> tsan_report.txt
```

### Captured report (first warning, from [tsan_report.txt](tsan_report.txt))

```
==================
WARNING: ThreadSanitizer: data race (pid=84)
  Read of size 8 at 0x000000404058 by thread T2:
    #0 worker /lab/race.c:18 (race_tsan+0x4011b4)
    #1 <null> <null> (libtsan.so.2+0x4bd49)

  Previous write of size 8 at 0x000000404058 by thread T1:
    #0 worker /lab/race.c:18 (race_tsan+0x4011e5)
    #1 <null> <null> (libtsan.so.2+0x4bd49)

  Location is global 'counter' of size 8 at 0x000000404058 (race_tsan+0x404058)

  Thread T2 (tid=87, running) created by main thread at:
    #0 pthread_create <null> (libtsan.so.2+0x566a6)
    #1 main /lab/race.c:29 (race_tsan+0x40122c)

  Thread T1 (tid=86, finished) created by main thread at:
    #0 pthread_create <null> (libtsan.so.2+0x566a6)
    #1 main /lab/race.c:29 (race_tsan+0x40122c)

SUMMARY: ThreadSanitizer: data race /lab/race.c:18 in worker
==================
```

The second warning has the same shape, between a **write** by T3 and a previous **write** by T2. The report ends with `ThreadSanitizer: reported 2 warnings`.

### Interpretation

| Item | Observation |
|---|---|
| Type of access #1 and source line | **Read** of size 8 by thread T2 in `worker`, race.c:18 (the `counter++` loop). Warning 2: **write** by T3, same line. |
| Type of access #2 ("Previous") and source line | **Write** of size 8 by thread T1 in `worker`, race.c:18. Warning 2: write by T2, same line. |
| Memory location reported | global `counter` (8 bytes at 0x404058) |
| Threads involved and where created | T1, T2 (and T3 in warning 2). All were created by the main thread in `main` at race.c:29 (`pthread_create`). |
| Number of warnings in one run | 2 |
| Final counter value (TSan build) | 400,000 (expected 400,000), which happened to be correct |

Across 3 TSan runs, the report and the final value did not change: each run printed 400,000 and 2 warnings on line 18. Only the pid and thread IDs differed.

Note on the line number: `counter++` is on line 19 of the source, and the `for` statement is on line 18. With `-O1`, the compiler merges the loop and the increment, so the debug info credits the load and store of `counter` to line 18. The report still names the variable (`global 'counter'`), which confirms that the conflicting access is the increment.

### Discussion

**1. Which line did TSan blame? Why are both accesses on the same line?**
TSan blamed `race.c:18` in `worker`, which is the `counter++` loop (see the note above). Both conflicting accesses are on the same line because every thread runs the *same* function. The race is not between two different statements. It is between two instances of the same `counter++` running in different threads, one thread's load or store against another thread's store.

**2. How can TSan flag a race that did not corrupt the result?**
TSan does not look at the final value. It uses *happens-before* tracking. For every memory location it keeps a record of recent accesses (which thread, read or write, and that thread's logical clock). Synchronization such as mutex lock/unlock, thread create and join, and atomics updates these clocks. When a new access conflicts with an earlier one (same address, at least one write) and no synchronization orders the two, TSan reports a race. The check is on the ordering, not on the outcome. It detects that the accesses *could* have overlapped, even though in this run they did not overlap at the harmful moment. This is exactly what happened here: the result was 400,000, and TSan still reported 2 races.

**3. Symptom vs. cause: why is TSan needed in addition to testing output?**
Task 1's wrong total is the *symptom*. It appears only when the timing is unlucky, and it does not say where the problem is. TSan shows the *cause*: the exact variable (`counter`), the source line, the type of each access, and which threads were involved. Output testing is probabilistic. A racy program can pass many test runs (as in Task 2 and in the 100-increment case) and then fail in production under a different load. TSan finds the race in a single run, whether or not the output happened to be wrong.

**4. Why not use the TSan build for timing?**
TSan instruments every memory access and maintains shadow memory for it, which slows the program by roughly 5–15×. It also changes the timing, and therefore the contention, between threads. Our TSan build of the `__sync` benchmark took about 95 ms for 400,000 increments (about 237 ns each). The `-O2` build took about 62 ms for 4,000,000 increments (about 15 ns each), so the TSan build was about 15× slower per increment. A timing from the TSan build would mostly measure TSan's own overhead, not the cost of the mutex or the atomic. TSan builds also use `-O1` instead of `-O2`, so the comparison would not be fair either.

---

## Task 3: Fixing the Race with a Mutex (Critical Section)

A global `pthread_mutex_t counter_lock = PTHREAD_MUTEX_INITIALIZER` was added. In the worker, only the `counter++` statement is wrapped in `pthread_mutex_lock` / `pthread_mutex_unlock`, not the whole loop:

```c
for (int i = 0; i < INCREMENTS_PER_THREAD; i++) {
    pthread_mutex_lock(&counter_lock);
    counter++; /* critical section */
    pthread_mutex_unlock(&counter_lock);
}
```

### Results

| Run | Expected Value | Actual Value | Correct? (Y/N) | TSan Warnings (count) |
|---|---|---|---|---|
| 1 | 4,000,000 | 4,000,000 | Y | n/a |
| 2 | 4,000,000 | 4,000,000 | Y | n/a |
| 3 | 4,000,000 | 4,000,000 | Y | n/a |
| 4 | 4,000,000 | 4,000,000 | Y | n/a |
| 5 | 4,000,000 | 4,000,000 | Y | n/a |
| 6 | 4,000,000 | 4,000,000 | Y | n/a |
| 7 | 4,000,000 | 4,000,000 | Y | n/a |
| 8 | 4,000,000 | 4,000,000 | Y | n/a |
| 9 | 4,000,000 | 4,000,000 | Y | n/a |
| 10 | 4,000,000 | 4,000,000 | Y | n/a |
| TSan build | 400,000 | 400,000 | Y | **0** |

### Discussion

**1. Was the value correct in all 10 runs? Why is this not conclusive, and what did TSan add?**
Yes, all 10 runs gave exactly 4,000,000. On its own this is not proof. Task 2 showed that a racy program can also print the correct total, so 10 correct runs could in principle be luck. The TSan run of the fixed program reported **0 warnings**. This means that every pair of accesses to `counter` was ordered by synchronization: each unlock *happens-before* the next lock of the same mutex. Since TSan tracks ordering and not results, a clean report is much stronger evidence than a correct total.

**2. Why is it safe for main() to read counter after the joins without the lock?**
`pthread_join()` does not return until the target thread has finished, and POSIX guarantees that all of that thread's memory writes are visible to the thread that joined it. In other words, the join creates a happens-before edge, just like unlock → lock does. After the last join, no other thread exists that could still write `counter`, so `main`'s read cannot race with anything.

**3. What if only three of four threads used the lock?**
The program would be incorrect again. A mutex only works if *every* access to the shared data goes through it. The fourth thread's bare `counter++` would ignore the lock, and its load/add/store could interleave with a locked thread's critical section. Updates would be lost again (probably fewer than in Task 1), and TSan would report a data race between the unlocked access and the locked ones.

**4. Lock per increment vs. lock around the whole loop: trade-off?**
Both placements are correct. Locking around each increment keeps the critical section as small as possible, so threads take turns at a fine grain. But it costs 4,000,000 lock/unlock pairs, and the lock is heavily contended. Locking around the whole loop needs only 4 lock/unlock pairs, so it has almost no lock overhead. But one thread holds the lock for its entire loop, so the threads run strictly one after another and there is no parallelism at all. In this program the only work *is* the shared increment, so neither placement gives a speed-up. The best design is to avoid sharing: each thread counts into a local variable and adds it to the total once at the end (as in Lab 03 Task 2).

---

## Task 4: Atomic Increment vs. Critical Section

[counter_bench.c](counter_bench.c) selects the increment method at compile time: `-DUSE_MUTEX` (lock/unlock around `counter++`), `-DUSE_SYNC` (`__sync_fetch_and_add`) or `-DUSE_C11` (`atomic_fetch_add` on an `atomic_long`). Only thread creation, the work and joining are timed, using `clock_gettime(CLOCK_MONOTONIC)`. All versions were built with `-O2 -std=gnu11` and without TSan.

### Results (4 threads × 1,000,000 increments per thread)

| Run | Mutex Time (ms) | __sync Atomic Time (ms) | C11 Atomic Time (ms) |
|---|---|---|---|
| 1 | 171.16 | 64.42 | 66.00 |
| 2 | 170.92 | 59.10 | 63.11 |
| 3 | 176.40 | 61.52 | 58.41 |
| 4 | 166.06 | 62.55 | 66.50 |
| 5 | 172.50 | 60.55 | 68.84 |
| **Average** | **171.41** | **61.63** | **64.57** |
| Correct in all runs? | Y | Y | Y |

**Ratio (average mutex time / average atomic time):** **2.78** (`__sync`), 2.65 (C11)
**Number of threads and increments per thread used:** 4 threads × 1,000,000 increments (4,000,000 total)

**TSan check of the atomic versions** (100,000 increments per thread): `USE_SYNC`: 0 warnings, `USE_C11`: 0 warnings. Both printed the correct total of 400,000.

### Thread-count sweep (for Q5, 1,000,000 increments per thread, average of 3 runs)

| Threads | Mutex (ms) | __sync Atomic (ms) | Ratio | Mutex ns/increment | Atomic ns/increment |
|---|---|---|---|---|---|
| 1 | 15.58 | 6.17 | 2.53 | 15.6 | 6.2 |
| 4 | 171.41 | 61.63 | 2.78 | 42.9 | 15.4 |
| 8 (= cores) | 490.70 | 134.88 | 3.64 | 61.3 | 16.9 |
| 16 (> cores) | 1329.25 | 269.71 | 4.93 | 83.1 | 16.9 |

### Discussion

**1. Did the atomic versions always give the correct total? Compare with Task 1.**
Yes. Both `__sync_fetch_and_add` and C11 `atomic_fetch_add` gave exactly 4,000,000 in all 5 runs, and TSan reported no races. The unsynchronized Task 1 program lost 63–72% of its increments and never gave the right answer. The only difference is that each increment is now a single indivisible read-modify-write, so no other thread can slip in between the load and the store.

**2. Which was faster, by what factor, and why?**
The atomic versions were faster: about 62–65 ms vs. about 171 ms, roughly **2.7–2.8×**. The two atomic versions performed almost the same (their individual run times overlap), since both compile to the same `lock xadd` instruction on x86. There are two reasons why the atomic is cheaper:
- **Fewer operations per increment.** An atomic increment is one hardware instruction. A mutex needs a function call to `pthread_mutex_lock` (which itself uses an atomic compare-and-swap), then the increment, then a call to `pthread_mutex_unlock` (another atomic operation). That is at least two atomic operations plus call overhead to protect one addition.
- **No blocking.** When the mutex is contended, glibc makes waiting threads sleep in the kernel with the `futex` system call and wakes them later. This involves system calls and context switches that cost microseconds. An atomic never blocks: the hardware simply serializes the instructions. A thread can also be preempted while holding the mutex, and then every other thread has to wait.

**3. Why is even the atomic version much slower than one thread incrementing an unshared variable?**
Because all threads write the *same memory location*, which lives in one cache line. To write it, a core must have that cache line in the exclusive (Modified) state in its own cache. So for every increment, the line has to be moved from the core that wrote it last, invalidating the other copies. This "cache-line ping-pong" costs tens of nanoseconds per transfer, and the increments are effectively serialized by the cache-coherence protocol. Adding threads adds no parallelism, only more coherence traffic. This shows up in our data: one thread does an atomic increment in about 6.2 ns, but with 4 or more threads each increment costs about 15–17 ns. So 4 threads needed about 10× the time of 1 thread to do 4× the work. A plain unshared `long` in a register would cost well under 1 ns per increment, and at `-O2` the compiler could even replace the loop with a single addition.

**4. What update can a mutex make safe but an atomic cannot?**
Any update that must change several variables together as one unit. Example: a bank transfer `from -= amount; to += amount;` with the rule that the total balance never changes. Making each line a separate atomic operation makes each variable individually correct, but another thread can run between the two operations. It would then see money that has left `from` but not yet arrived in `to`, or two transfers could interleave and break a check such as `if (from >= amount)`. The check-and-update of two variables needs a critical section:

```c
pthread_mutex_lock(&bank_lock);
if (from >= amount) { from -= amount; to += amount; }
pthread_mutex_unlock(&bank_lock);
```

The mutex makes the whole block appear indivisible to other threads, which an atomic on a single variable cannot do. Other examples are inserting a node into a linked list while also updating its size, or updating a `sum` and a `count` that must stay consistent.

**5. How does the ratio change with 1 thread and with more threads than cores?**
- *Prediction:* With 1 thread there is no contention. The mutex always takes its fast path (one uncontended atomic to lock and one to unlock), so it should cost only about 2–3× an atomic increment. With more threads than cores, the mutex should get much worse. Threads begin to sleep in the kernel (futex), and the lock holder can be preempted by the scheduler while holding the lock, so every waiting thread stalls until it runs again (lock convoys). The atomic cannot be "held", so it only suffers from cache-line contention.
- *Measured:* The ratio rose steadily with the number of threads: **2.53× (1 thread) → 2.78× (4) → 3.64× (8 = number of cores) → 4.93× (16, more than cores)**. The atomic's cost per increment stayed nearly flat once contention started (15.4–16.9 ns). The mutex's cost kept growing (16 → 43 → 61 → 83 ns), confirming that its extra cost comes from contention and blocking, not just from the lock/unlock instructions.

---

## Conclusion

The unsynchronized counter lost about 68% of its updates and gave a different wrong answer on every run, because `counter++` is a non-atomic load/add/store. ThreadSanitizer pinpointed the cause: two unsynchronized accesses to the global `counter` in `worker`, made by threads created in `main`. It did so even in runs where the printed total was correct. Wrapping the increment in a mutex made every run correct, and TSan confirmed there were no races. Replacing the mutex with an atomic increment was equally correct and about 2.8× faster with 4 threads. The advantage grew to 4.9× with 16 threads, because the atomic never blocks, while the mutex suffers more as contention increases. Both remain far slower than unshared work, because all threads fight over one cache line. Atomics are the right tool for a single variable. A mutex is needed when several pieces of shared state must change together.
