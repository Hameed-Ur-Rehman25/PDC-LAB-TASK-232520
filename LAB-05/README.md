# Lab 05: Synchronization Mechanisms

**Name:** Hameed Ur Rehman &nbsp;|&nbsp; **Roll No:** 232520 &nbsp;|&nbsp; **Class:** BSCS VII C &nbsp;|&nbsp; **Course:** CS426L Introduction to Parallel & Distributed Computing

**Test machine:** Intel Core i7-1068NG7 @ 2.30 GHz, 8 logical cores (`nproc` = 8), macOS host.
**Toolchain:** GCC 14.4.0 with ThreadSanitizer, running in a Linux container (`gcc:14` Docker image) instead of WSL, the same setup as Lab 04.
Raw console output from every run is in [output.txt](output.txt). All numbers in this report are copied from that file.

## Project Structure

| File | Task |
|---|---|
| [buffer_mutex.c](buffer_mutex.c) | Task 1: Mutex-protected bounded buffer (1 producer, 1 consumer, polling) |
| [buffer_sem.c](buffer_sem.c) | Task 2: Multiple producers and consumers with counting semaphores |
| [cond_signal.c](cond_signal.c) | Task 3: Condition-variable signaling |
| [phase_barrier.c](phase_barrier.c) | Task 4: Reusable phase barrier |
| [lockfree_stack.c](lockfree_stack.c) | Task 5 (optional): Lock-free stack with compare-and-swap |
| [run_all.sh](run_all.sh) | Builds and runs every task in order and prints all results |

### How to Run

On WSL / Linux:

```bash
cd LAB-05
bash run_all.sh | tee output.txt
```

On macOS (Docker Desktop running):

```bash
cd LAB-05
docker run --rm -v "$PWD":/lab gcc:14 bash /lab/run_all.sh | tee output.txt
```

The full run takes about 5 minutes. Most of that is the 4 / 4 / 1 configuration of Task 2.

The experiments that the task sheet describes as temporary source edits are compile-time switches here, so the submitted source is always the correct version and every experiment can be repeated:

| Switch | Experiment |
|---|---|
| `-DSWAP_LOCK_ORDER` ([buffer_sem.c](buffer_sem.c)) | Task 2 deadlock experiment: consumer locks `buf_lock` before `sem_wait(&full_slots)` |
| `-DNO_SIGNAL` ([cond_signal.c](cond_signal.c)) | Task 3 lost-signal experiment: `publish()` sets the flag but wakes nobody |
| `-DSIGNAL_ONE` ([cond_signal.c](cond_signal.c)) | Task 3 Q5: `pthread_cond_signal` instead of `pthread_cond_broadcast` |
| `-DNO_BARRIER` ([phase_barrier.c](phase_barrier.c)) | Task 4: `barrier_wait()` does nothing |
| `-DUSE_MUTEX` ([lockfree_stack.c](lockfree_stack.c)) | Task 5 Q5: mutex-protected stack for comparison |

---

## Task 1: Mutex-Protected Bounded Buffer

One producer inserts the integers 0 … 999,999 into a circular buffer and one consumer removes them. `buffer`, `in_pos`, `out_pos` and `count` are guarded by one mutex. A thread that cannot proceed unlocks, counts a retry and calls `sched_yield()`:

```c
pthread_mutex_lock(&lock);
if (count < BUFFER_SIZE) {
    buffer[in_pos] = item;               /* test and insert in the same critical section */
    in_pos = (in_pos + 1) % BUFFER_SIZE;
    count++;
    pthread_mutex_unlock(&lock);
    item++;
} else {
    pthread_mutex_unlock(&lock);
    producer_retries++;
    sched_yield();
}
```

The consumer compares every removed item with the value it expects next (`order_errors`) and adds it to a checksum, which is compared with 0 + 1 + … + 999,999.

### Results (buffer size 8, 1,000,000 items, `-O2`)

| Run | Items | Checksum OK (Y/N) | Order errors | Producer retries | Consumer retries | Time (ms) |
|---|---|---|---|---|---|---|
| 1 | 1,000,000 | Y | 0 | 134,747 | 135,378 | 143.37 |
| 2 | 1,000,000 | Y | 0 | 132,594 | 134,146 | 135.44 |
| 3 | 1,000,000 | Y | 0 | 141,865 | 160,313 | 165.96 |
| 4 | 1,000,000 | Y | 0 | 139,182 | 138,435 | 150.98 |
| 5 | 1,000,000 | Y | 0 | 133,661 | 134,439 | 130.48 |
| **Average** | 1,000,000 | Y | 0 | **136,410** | **140,542** | **145.25** |

Average CPU time was 282.12 ms, about twice the wall time: both threads were running the whole time.

### Effect of buffer size on retries (one run each)

| Buffer size | Producer retries | Consumer retries | Time (ms) |
|---|---|---|---|
| 1 | 1,024,056 | 1,036,933 | 807.85 |
| 8 | 134,747 | 135,378 | 143.37 |
| 64 | 23,639 | 20,080 | 122.23 |

**Buffer size and number of items used for the first table:** buffer size 8, 1,000,000 items
**TSan build** (`-DITEMS=20000`): checksum OK, 0 order errors, **0 warnings**.

### Discussion

**1. Was every item received in order, and did the checksum always match? Why does one producer and one consumer on a circular buffer preserve FIFO order?**
Yes. All 5 runs (and the runs with buffer sizes 1 and 64) had a matching checksum and 0 order errors. The producer is the only thread that advances `in_pos`, and it writes items in increasing order into consecutive slots. The consumer is the only thread that advances `out_pos`, and it reads the slots in the same consecutive order. `count` stops the producer from overwriting a slot that has not been read and stops the consumer from reading a slot that has not been written. So the consumer visits the slots in exactly the order in which they were filled, and the first item in is the first item out.

**2. Why must the test of `count` and the insertion happen inside the same critical section?**
The test is only valid while the lock is held. If the producer tested `count < BUFFER_SIZE`, unlocked, and then locked again to insert, the state could change between the two steps and the insertion would act on stale information (a "check-then-act" race). With a single producer the count can only go down in that gap, so this particular program would survive. With two producers it breaks: both see `count == 7` with a capacity of 8, both decide there is room, and both insert. The second insertion overwrites the oldest unread item, `count` becomes 9, and one item is lost. The same applies to two consumers that both see `count == 1` and both remove: one of them reads a stale slot, so an item is duplicated. Keeping the test and the update in one critical section makes them a single indivisible step.

**3. Compare the retry counts for buffer sizes 1, 8 and 64. What do the retries represent, and why does `sched_yield()` reduce but not remove the waste?**
Retries fell sharply as the buffer grew: about 1,030,000 per thread with size 1 (roughly one failed attempt per item), about 135,000 with size 8, and about 22,000 with size 64. The time followed: 808 ms, 143 ms, 122 ms. A larger buffer absorbs short differences in speed between the two threads, so each thread finds the buffer full or empty less often. With size 1 the threads must alternate strictly, so almost every item costs a failed attempt on one side.
Each retry is wasted CPU time: one lock, one test, one unlock and a system call that achieved nothing. `sched_yield()` gives up the rest of the time slice so another runnable thread can use the core. That helps when the other thread is waiting for this core. But the thread stays *runnable*. With 8 cores and 2 threads there is usually nothing else to run, so the call returns immediately and the thread polls again. The wait is still a busy loop, only a slightly more polite one. The CPU time of about 2× the wall time shows that both cores stayed busy throughout. Removing the waste requires the thread to *block* until the state changes, which is what Tasks 2 and 3 do.

**4. What if the producer slept while still holding the lock when the buffer is full?**
The consumer needs the same lock to remove an item, so it would block in `pthread_mutex_lock` for the whole sleep. The buffer therefore stays full while the producer sleeps. If the producer sleeps once and then unlocks, the program becomes extremely slow, because progress is only possible in the short windows between sleeps. If it keeps the lock and waits for space (sleeping in a loop until `count < BUFFER_SIZE`), it is a deadlock: the producer waits for the consumer to make room, and the consumer waits for the producer to release the lock. This is why a waiting thread must release the lock, and why `pthread_cond_wait` (Task 3) releases the mutex atomically while it sleeps.

---

## Task 2: Multiple Producers and Consumers with Semaphores

`empty_slots` (initially `BUFFER_SIZE`) counts free slots and `full_slots` (initially 0) counts stored items. `buf_lock` protects only the indices. Both sides use the order semaphore wait → mutex lock → update → mutex unlock → semaphore post:

```c
static void put_item(long item)
{
    sem_wait(&empty_slots);          /* blocks while the buffer is full */
    pthread_mutex_lock(&buf_lock);
    buffer[in_pos] = item;
    in_pos = (in_pos + 1) % BUFFER_SIZE;
    pthread_mutex_unlock(&buf_lock);
    sem_post(&full_slots);
}
```

Each producer sends 100,000 items with ids that are unique across producers. Consumers count every received id in `seen[]`. After the producers are joined, `main` inserts one stop item per consumer. An id with `seen == 0` is lost and an id with `seen > 1` is duplicated.

### Results (100,000 items per producer, `-O2`, three runs each)

| Producers / consumers / capacity | Lost | Duplicated | Run 1 (ms) | Run 2 (ms) | Run 3 (ms) | Average (ms) |
|---|---|---|---|---|---|---|
| 1 / 1 / 8 | 0 | 0 | 1117.14 | 1082.12 | 1124.24 | **1107.83** |
| 2 / 2 / 8 | 0 | 0 | 1974.45 | 1976.82 | 1950.57 | **1967.28** |
| 4 / 4 / 8 | 0 | 0 | 3584.73 | 4421.81 | 5567.79 | **4524.78** |
| 4 / 4 / 1 | 0 | 0 | 57648.62 | 40579.11 | 41165.17 | **46464.30** |
| 4 / 4 / 64 | 0 | 0 | 520.48 | 580.77 | 500.51 | **533.92** |
| 4 / 2 / 8 | 0 | 0 | 4514.24 | 5124.19 | 5936.74 | **5191.72** |

Lost and duplicated were 0 in all 18 runs.

**TSan build** (4 / 4 / 8, `-DITEMS_PER_PRODUCER=5000`): lost = 0, duplicated = 0, **0 warnings**.

**Deadlock experiment** (`buffer_sem_swapped`, built with `-DSWAP_LOCK_ORDER`): the program printed nothing and was killed by `timeout 5` (exit code 124). It made no progress at all. The correct order is the default build.

### Discussion

**1. Roles of the two semaphores and their initial values.**
`empty_slots` counts the free slots. A producer must take one before it inserts, so producers block when the buffer is full. `full_slots` counts the stored items. A consumer must take one before it removes, so consumers block when the buffer is empty. Each side posts the *other* semaphore after its update: an insertion creates one item, a removal creates one free slot. At the start the buffer is empty, so there are `BUFFER_SIZE` free slots and 0 items, which gives the initial values. At every moment `empty_slots + full_slots` (plus the operations in progress) equals the capacity, so the buffer can neither overflow nor underflow.

**2. Why is a mutex still needed, and would it be needed with one producer and one consumer?**
The semaphores only limit *how many* threads may be inside. With capacity 8, `empty_slots` can let up to 8 producers in at once. Two of them could read the same `in_pos`, write to the same slot and then both advance the index: one item is overwritten (lost) and one slot holds stale data that is consumed later (duplicated). `in_pos = (in_pos + 1) % BUFFER_SIZE` is a read-modify-write just like `counter++` in Lab 04. The same applies to consumers and `out_pos`. The mutex makes the index update exclusive.
With exactly one producer and one consumer the mutex is not required for this design. `in_pos` is touched only by the producer and `out_pos` only by the consumer, and there is no shared `count` variable because the semaphores do the counting. The semaphores also guarantee that the two threads never work on the same slot: the consumer can only reach a slot after the producer has posted `full_slots` for it, and that post happens after the write.

**3. Why does the program stop when a consumer takes the mutex before waiting on `full_slots`?**
1. The buffer starts empty, so `full_slots` is 0.
2. A consumer locks `buf_lock`.
3. It calls `sem_wait(&full_slots)`. The value is 0, so the consumer goes to sleep *while still holding* `buf_lock`.
4. A producer takes a free slot with `sem_wait(&empty_slots)` and then calls `pthread_mutex_lock(&buf_lock)`. The lock is held by the sleeping consumer, so the producer blocks.
5. Only a producer can post `full_slots`, and no producer can get past the mutex. The consumer waits for a producer, and every producer waits for the consumer. The other consumers are blocked on the mutex as well.

This is a circular wait, i.e. a deadlock. The rule is to never block on a semaphore while holding the mutex: wait for the resource first, then take the lock only for the short index update.

**4. How did the time change with the capacity and with the number of threads?**

| Configuration | Total items | Time per item |
|---|---|---|
| 1 / 1 / 8 | 100,000 | 11.1 µs |
| 2 / 2 / 8 | 200,000 | 9.8 µs |
| 4 / 4 / 8 | 400,000 | 11.3 µs |
| 4 / 2 / 8 | 400,000 | 13.0 µs |
| 4 / 4 / 1 | 400,000 | 116 µs |
| 4 / 4 / 64 | 400,000 | 1.3 µs |

*Capacity* had by far the largest effect: with 4 / 4 threads, capacity 1 took 46.5 s, capacity 8 took 4.5 s and capacity 64 took 0.53 s, a factor of about 87 between the extremes. With capacity 1 only one item can be in transit, so almost every `sem_wait` finds the value 0 and the thread goes to sleep in the kernel. Every item then costs several sleep/wake-up pairs and context switches, and 8 threads compete for a single slot. With capacity 64 most `sem_wait` calls succeed immediately without a system call, so threads rarely block.
*More threads did not make it faster.* The time per item stayed around 10–13 µs for capacity 8 whether 2, 4, 6 or 8 threads were used, so the total time simply grew with the number of items. All threads share one buffer and one mutex, so the hand-over is serialized. Extra threads only add contention on `buf_lock` and more wake-ups. The CPU time shows this: 4 / 4 / 8 used about 11–17 s of CPU for 3.6–5.6 s of wall time. The 4 / 2 / 8 configuration was slightly slower than 4 / 4 / 8, because two consumers cannot drain as fast as four producers fill, so the producers spend more time blocked on `empty_slots`.
The run-to-run variation was also large for the contended configurations (for example 3.6 s to 5.6 s for 4 / 4 / 8), because the time depends on how the scheduler happens to order the wake-ups.

**5. Compare the 1 / 1 / 8 semaphore version with the Task 1 polling version. Why does blocking differ from polling?**
For a fair comparison the semaphore version was also run with 1,000,000 items (same workload as Task 1):

| Version (1 producer, 1 consumer, capacity 8, 1,000,000 items) | Wall time (ms) | CPU time (ms) |
|---|---|---|
| Task 1: mutex + polling (average of 5) | 145.25 | 282.12 |
| Task 2: semaphores + blocking (average of 3) | 12,657.86 | 10,437.13 |

On this machine the polling version was much faster, about 87× in wall time, and it also used less CPU time in total. This is the opposite of the usual expectation, and the reason is the workload. Both threads do almost no work per item and each has its own core, so the wait for the other thread is extremely short (well under a microsecond). A polling thread that finds the buffer full simply tries again a moment later and succeeds. A blocking thread must be put to sleep by the kernel and woken up again by the other thread's `sem_post`. Each sleep/wake-up pair is a system call plus a context switch, which costs several microseconds, much more than the wait itself. The measured cost of about 11–13 µs per item fits this. The CPU time of the blocking version is mostly kernel time spent on these system calls, and it is probably higher here than on native Linux because the container runs inside a virtual machine.
The difference in principle is what a waiting thread does. A polling thread stays runnable and burns CPU for the whole wait. A blocked thread is removed from the run queue and uses no CPU until it is woken. So polling is cheap when waits are very short and a core is free, and its cost grows with the length of the wait. Blocking has a fixed cost per wait and costs nothing while waiting. Task 3 shows the other end of the range: for a 1-second wait, blocking used 0.61 ms of CPU and polling used 1000 ms. This is also why real mutexes spin briefly before they sleep.

**6. Is FIFO order guaranteed as seen by multiple consumers? Why does `seen` check for lost and duplicated items instead of order?**
The *buffer* is still FIFO: items leave in the order in which they were inserted. But there is no global order to check. First, the insertion order itself is not fixed: items from different producers interleave differently in every run. Second, after a consumer has removed an item and released the mutex, it can be descheduled before it processes the item, while another consumer removes and processes a later one. So the order in which items are *processed* can differ from the order in which they were removed. Each single consumer still sees the items of one particular producer in increasing order, but nothing more can be said.
What must hold in every correct run is that each item is delivered exactly once. `seen[id]` checks precisely that: 0 means lost, more than 1 means duplicated. This property does not depend on the interleaving, so it can be checked in every configuration.

---

## Task 3: Condition-Variable Signaling

A worker locks the mutex and waits on a condition variable until `data_ready` is set. `publish()` sets the value and the flag under the mutex and wakes the waiting workers:

```c
pthread_mutex_lock(&lock);
while (!data_ready) {            /* re-check the predicate after every wake-up */
    times_blocked[id]++;
    pthread_cond_wait(&ready_cond, &lock);
}
received[id] = shared_value;
pthread_mutex_unlock(&lock);
```

```c
pthread_mutex_lock(&lock);
shared_value = value;
data_ready = 1;
pthread_cond_broadcast(&ready_cond);   /* wake every waiting worker */
pthread_mutex_unlock(&lock);
```

In scenarios A, C and D, `main` starts the workers, sleeps for `DELAY_MS` = 1000 ms and then publishes the value 42. In scenario B (`-DSIGNAL_FIRST`) it publishes before the workers are created. In scenario D (`-DUSE_SPIN`) the worker polls the flag in a busy loop (lock, test, unlock) instead of waiting.

### Scenario results

| Scenario | Times blocked in wait | Value received | Wall time (ms) | CPU time (ms) |
|---|---|---|---|---|
| A: cond, 1 worker | 1 | 42 | 1000.93 | 0.61 |
| B: signal-first | 0 | 42 | 0.52 | 0.59 |
| C: cond, 3 workers | 1 / 1 / 1 (each worker) | 42 / 42 / 42 | 1001.07 | 1.73 |
| D: spin (polling) | 0 (42,917,429 polls) | 42 | 1000.60 | 1000.45 |

**TSan build** (variant A, `-DDELAY_MS=200`): blocked = 1, value = 42, **0 warnings**.

**Lost-signal experiment** (`-DNO_SIGNAL`): the program printed nothing and was killed by `timeout 5` (exit code 124). The flag was set, but the worker never woke up.

### Discussion

**1. Why a `while` loop instead of an `if`?**
A wake-up is only a hint that the predicate *may* now be true. The worker must check it again in these situations:
- **Spurious wake-ups.** POSIX allows `pthread_cond_wait` to return even though nobody signaled. With an `if`, the worker would continue and read `shared_value` before it was published.
- **Another thread got there first.** Between the signal and the moment the woken thread re-acquires the mutex, a different thread can take the lock and change the state. In a producer-consumer queue, another consumer can remove the item, so the predicate is false again when the woken thread finally runs.
- **Broadcast for a different reason.** With `pthread_cond_broadcast`, or when one condition variable is shared by several predicates, all waiters wake up, but the state may only allow some of them to proceed.

With `while`, the worker simply goes back to sleep in all of these cases. The loop also handles scenario B for free: the predicate is already true, so the body never runs.

**2. What does `pthread_cond_wait()` do with the mutex, and why must it be held?**
It atomically releases the mutex and puts the thread to sleep on the condition variable. When the thread is woken, it re-acquires the mutex before the call returns, so the thread holds the lock again when it re-checks the predicate.
The mutex must be held because the predicate is shared state, and because the "test, then sleep" sequence must not be interrupted. If the worker tested the flag without the lock, `publish()` could set the flag and signal *between* the worker's test and its sleep. The signal would find no waiter and be lost, and the worker would then sleep forever. Because the worker holds the mutex from the test until `pthread_cond_wait` has put it on the wait queue, and `publish()` needs the same mutex to change the flag, the publisher can only run before the test or after the worker is really waiting.

**3. In scenario B the worker never blocks, and in the lost-signal experiment it never wakes up. Explain both.**
In scenario B the broadcast happened before any worker existed. A condition variable has no memory: a signal with no waiter is simply discarded. The worker still got the value, because it tests the *flag* first. The flag was already 1, so it skipped the wait (blocked = 0) and finished in 0.52 ms.
In the lost-signal experiment the worker was already asleep when the flag was set. The flag became 1, but a sleeping thread does not look at the flag. It needs the signal to be woken and to run its check, so without the signal it slept forever.
Together these show that the two parts have different jobs. The flag is the *state*: it remembers that the event happened, and it is what the worker's decision is based on. The signal is only the *wake-up call* that tells a sleeping thread to look at the state again. The flag without the signal leaves waiters asleep, and the signal without the flag would be lost whenever it arrives early.

**4. Compare the CPU time of scenario A with scenario D.**
Both waited about 1000 ms of wall time. Scenario A used 0.61 ms of CPU and scenario D used 1000.45 ms, about 1600× more. The blocked worker is taken off the run queue by the kernel and executes no instructions until the broadcast wakes it, so its cost is one sleep and one wake-up no matter how long it waits. The spinning worker stays runnable and occupied one core completely for the whole second. It locked, tested and unlocked about 42.9 million times, and every one of those checks was wasted except the last. The cost of polling grows with the waiting time, and it also takes a core away from other threads and repeatedly takes the mutex that the publisher needs.

**5. Why is `pthread_cond_signal()` not sufficient for three workers?**
`pthread_cond_signal` wakes at least one waiter, and in practice exactly one. In scenario C all three workers wait for the same one-time event and `publish()` is called only once. With `signal`, one worker would wake up and the other two would keep sleeping, since no further signal will ever come. This was tested with `-DNUM_WORKERS=3 -DSIGNAL_ONE`: the program hung and was killed by `timeout 5`. `pthread_cond_broadcast` wakes *all* waiters. They re-acquire the mutex one after another, each one sees `data_ready == 1` and continues, which is why all three workers show blocked = 1 and value = 42. `signal` is the right choice when one event can be used by only one thread (one new item in a queue). `broadcast` is needed when the change is relevant to every waiter.

---

## Task 4: Phase Barrier for Worker Threads

The workers update a ring of 4096 cells for 10,000 rounds. Each worker owns one slice. A round has two phases, each ended by a barrier:

```c
for (int round = 0; round < ROUNDS; round++) {
    for (int i = first; i < last; i++)
        next[i] = next_value(cur, i, round);   /* reads the neighbours' cells in cur[] */
    barrier_wait(&phase_barrier);              /* everyone has finished reading cur[] */

    for (int i = first; i < last; i++)
        cur[i] = next[i];
    barrier_wait(&phase_barrier);              /* everyone has finished writing cur[] */
}
```

The new value of a cell depends on its left and right neighbours, so the cells at a slice boundary read data that belongs to another worker. The barrier is built from a mutex and a condition variable:

```c
static void barrier_wait(barrier_t *b)
{
    pthread_mutex_lock(&b->lock);
    long my_generation = b->generation;
    if (++b->waiting == b->parties) {
        /* last arrival: open the barrier and reset it for the next use */
        b->generation++;
        b->waiting = 0;
        pthread_cond_broadcast(&b->all_arrived);
    } else {
        while (my_generation == b->generation)
            pthread_cond_wait(&b->all_arrived, &b->lock);
    }
    pthread_mutex_unlock(&b->lock);
}
```

`main` computes the same rounds single-threaded first and compares the two grids cell by cell.

### Results (4 workers, 10,000 rounds, 4096 cells, `-O2`)

| Run | With barrier: correct? (Y/N) | With barrier: time (ms) | No barrier: correct? (Y/N) | No barrier: mismatching cells |
|---|---|---|---|---|
| 1 | Y | 3174.65 | N | 4096 / 4096 |
| 2 | Y | 3543.26 | N | 4096 / 4096 |
| 3 | Y | 3323.41 | N | 4096 / 4096 |
| 4 | Y | 3192.01 | N | 4096 / 4096 |
| 5 | Y | 3407.33 | N | 4096 / 4096 |
| **Average** | Y | **3328.13** | N | 4096 / 4096 |

**TSan warnings on the barrier build:** 0 (`-DROUNDS=2000`, result correct) &nbsp;&nbsp; **Time with 8 workers (ms):** 21,809.79 (result correct) &nbsp;&nbsp; **nproc:** 8

The no-barrier runs took about 20 ms each.

### Discussion

**1. Why are two barrier calls needed in every round?**
A round has two phases that must not overlap between threads. In phase 1 every worker *reads* `cur[]`, including cells of its neighbours. In phase 2 every worker *writes* its slice of `cur[]`. The first barrier guarantees that all workers have finished reading the old values before anyone overwrites them. The second barrier guarantees that all workers have finished writing the new values before anyone starts reading them for the next round.
Without the second barrier, a fast worker would finish its copy and start phase 1 of the next round while a slower neighbour is still copying. At the slice boundary it would read a cell that still holds the value of the previous round, or it would read while the neighbour is writing. The result would depend on timing and the program would have a data race, exactly as in the no-barrier version, only less often.

**2. Why does the barrier use a generation number instead of only an arrival count?**
The barrier is reused 20,000 times, so a waiter needs a condition that says "*my* phase was released" and that stays true afterwards. An arrival count cannot provide that. The last thread to arrive resets `waiting` to 0 for the next use and broadcasts. A woken thread that checked a count-based predicate such as `waiting < parties` would be looking at the count of a later phase:
- A fast thread (often the releasing thread itself) can finish its next phase and call `barrier_wait` again before the slower waiters have re-acquired the mutex. `waiting` is then 1. A waiter from the previous phase that now re-checks sees "not everyone has arrived" and goes back to sleep. It never arrives for the new phase, so the count never reaches `parties` and all threads hang.
- A spurious wake-up could not be distinguished from a real release either.

The generation number solves this. Each waiter remembers the generation in which it arrived and waits until the generation has changed. The generation only increases, so once a phase is released it stays released for its waiters, no matter how quickly other threads start arriving for the next phase.

**3. What did the no-barrier version produce?**
It was wrong in every run: all 4096 of 4096 cells differed from the reference. The number of mismatching cells was the same each time, but the wrong values were not. The checksum of the final grid was different in all 5 runs (`5680d00b`, `642c29d9`, `f51d2919`, `4196a6c3`, `9155b8f7`), while the barrier version produced `abe2e000` every time. Without the barrier each worker runs its 10,000 rounds at its own pace, so a boundary cell is computed from a neighbour value that belongs to some other round. Which round depends on the scheduling, and the error then spreads by one cell per round until the whole grid is affected.
It would count as a data race even if the output happened to be right, because a data race is a property of the program and not of one run. Two threads access the same cell of `cur[]`, at least one of them writes, and nothing orders the two accesses. That is undefined behaviour in C, and a matching result would only mean that this run had a harmless interleaving. This is the same lesson as in Lab 04, where TSan reported a race in a run that printed the correct total.

**4. How did the time change with 8 workers, and why is a round as slow as the slowest thread?**
The time went from 3328 ms (4 workers) to 21,810 ms (8 workers), about 6.6× slower, for the same total work. The computation is tiny (the no-barrier version needs about 20 ms for all of it), so the time is almost entirely the cost of 20,000 barrier crossings: about 166 µs each with 4 workers and about 1090 µs each with 8. At every barrier all workers but one go to sleep and must be woken, and after the broadcast they all compete for the same mutex. With 8 workers on 8 logical cores (4 physical cores, shared with `main`, the virtual machine and the host), there is no spare core, so some worker regularly has to wait to be scheduled. This figure also varies a lot from run to run: an earlier complete run of the script on the same machine measured about 8.1 s for 8 workers and about 3.2 s for 4.
A barrier opens only when the *last* thread arrives, so every round lasts as long as its slowest participant, and the faster threads idle in the meantime. More workers means more chances that one of them is delayed in a given round, and that delay is charged to everybody. A barrier is only worth its cost when the work per phase is much larger than the cost of the barrier, which is not the case with 4096 cells.

**5. `pthread_barrier_t` versus the hand-built barrier.**
Two things the built-in barrier offers that this one does not:
- **A serial thread.** `pthread_barrier_wait` returns `PTHREAD_BARRIER_SERIAL_THREAD` to exactly one of the released threads. This makes it easy to run a single-threaded step once per phase, for example swapping two buffer pointers instead of copying.
- **A complete, standard interface.** `pthread_barrier_init` validates its arguments (a count of 0 gives `EINVAL`), `pthread_barrier_destroy` reports `EBUSY` if threads are still waiting, and barrier attributes allow a barrier to be shared between processes. The hand-built version checks nothing and works only inside one process.

Building one is still preferable when the platform has no `pthread_barrier_t` (it is an optional part of POSIX and macOS does not provide it), or when extra behaviour is needed: a timeout, a way to cancel or break the barrier, a number of parties that changes while running, or an action that must run under the lock at the moment the barrier opens.

---

## Task 5: Lock-Free Stack with Compare-and-Swap (Optional Extension)

The stack is a linked list whose `top` pointer is an `_Atomic(node_t *)`. `push()` and `pop()` use CAS retry loops:

```c
static void push(node_t *n)
{
    node_t *old_top = atomic_load(&top);
    do {
        n->next = old_top;           /* n is still private, so this write is safe */
    } while (!atomic_compare_exchange_weak(&top, &old_top, n));
}

static node_t *pop(void)
{
    node_t *old_top = atomic_load(&top);
    while (old_top != NULL &&
           !atomic_compare_exchange_weak(&top, &old_top, old_top->next))
        ;                            /* a failed CAS reloads old_top */
    return old_top;
}
```

Each of the 4 threads pushes its own 100,000 pre-allocated nodes and pops one node after every push, so pushes and pops from all threads interleave. Every popped node is counted in an atomic `seen[]` array.

### Results (4 threads × 100,000 nodes, `-O2`)

| Run | Nodes (total) | Lost | Duplicated | Stack empty (Y/N) | Time (ms) |
|---|---|---|---|---|---|
| 1 | 400,000 | 0 | 0 | Y | 67.95 |
| 2 | 400,000 | 0 | 0 | Y | 68.67 |
| 3 | 400,000 | 0 | 0 | Y | 73.62 |
| 4 | 400,000 | 0 | 0 | Y | 68.32 |
| 5 | 400,000 | 0 | 0 | Y | 66.09 |
| **Average** | 400,000 | 0 | 0 | Y | **68.93** |

**TSan build** (`-DNODES_PER_THREAD=5000`): lost = 0, duplicated = 0, stack empty, **0 warnings**.
**8 threads** (800,000 nodes): lost = 0, duplicated = 0, stack empty, 218.04 ms.

### Discussion

**1. Why does the CAS retry loop never lose an element? Why lock-free but not wait-free?**
A push prepares `n->next = old_top` privately and then asks the hardware to change `top` from `old_top` to `n` *only if `top` still equals `old_top`*. If another thread pushed or popped in between, `top` is different, the CAS fails and changes nothing, and the loop repeats with the new top. So a successful CAS always links the new node in front of the *current* list, and no update can overwrite another one. This is the difference from the lost update in Lab 04, where a store was made unconditionally from a stale value. Pop works the same way.
The stack is lock-free because a CAS can only fail when another thread's CAS succeeded. So in any period at least one thread completes its operation, and the system as a whole always makes progress, even if a thread is suspended in the middle of its operation. It is not wait-free, because there is no bound for an individual thread: an unlucky thread can lose the CAS again and again while others keep succeeding, so in theory it can starve.

**2. The ABA problem.**
CAS compares only the pointer value. Suppose a thread in `pop` reads `old_top = A` and `A->next = B` and is then paused. Other threads pop A, pop B, and push A again. `top` is A again, so the paused thread's CAS succeeds and sets `top = B`. But B is no longer in the stack: it may be in use elsewhere or freed. The stack is now corrupted, even though the CAS saw "no change".
It cannot occur in this test because a node is pushed exactly once and is never reused or freed. Once A has been popped, `top` can never be equal to A again, so "`top` still equals `old_top`" really does mean that the node is still on top with the same `next`. The risk would appear as soon as popped nodes were pushed again, or were freed and their memory returned by `malloc` at the same address for a new node. Common countermeasures are a version counter stored next to the pointer (tagged pointer), or a safe reclamation scheme.

**3. Why are popped nodes never freed here? Name a reclamation technique.**
Another thread may be in the middle of `pop`, holding a pointer to the same node, and about to read `old_top->next`. If the node were freed after being popped, that read would be a use-after-free, and reusing the memory would also bring back the ABA problem. In this program all nodes live in one array that is freed only after every thread has been joined. Real lock-free code uses techniques such as **hazard pointers** (a thread publishes the pointers it is using, and a node is freed only when no thread has published it), epoch-based reclamation, RCU, or a garbage collector.

**4. What does the "no lost, no duplicated, stack empty" check prove, and what not?**
It proves that in these runs every node that was pushed was popped exactly once and that nothing remained. That rules out the typical failures of a broken stack: a push overwritten by another push (lost node), one node returned by two pops (duplicate), and nodes left behind in a corrupted list.
It does not prove that the stack is correct in general. It is a test of some interleavings, not of all of them. It does not check LIFO order at all: a structure that returned nodes in any order would pass. It says nothing about ABA or memory reclamation, because this test never reuses or frees nodes. It also covers only one usage pattern and one architecture. The clean TSan run adds that there were no data races in the executed paths, but that is not a proof of correctness either.

**5. Comparison with a mutex-protected stack (`-DUSE_MUTEX`, same test).**

| Threads | Lock-free (ms) | Mutex (ms) |
|---|---|---|
| 4 (average of 5) | 68.93 | 81.42 |
| 8 (one run) | 218.04 | 208.88 |

There is no clear winner. With 4 threads the lock-free stack was about 15% faster in these runs, and with 8 threads the mutex version was slightly faster. In an earlier complete run of the script the order was the other way round with 4 threads (about 60 ms lock-free against about 40 ms mutex), so the difference is within the variation of this machine.
Lock-free does not always mean faster. Every operation of every thread still writes the same `top` pointer, so the cache line holding it moves between cores for every push and pop, in both versions. Under heavy contention the lock-free version also wastes work: a failed CAS means the attempt is thrown away and repeated. A mutex, in contrast, lets one thread do several operations in a row while the others sleep. What lock-free does guarantee is progress, not speed: no thread can block the others by being descheduled while holding a lock, there is no deadlock, and the structure is safe to use where blocking is not allowed.

---

## Conclusion

A mutex alone makes the bounded buffer correct, but a thread that must wait can only poll, which wasted up to one failed attempt per item. Counting semaphores let any number of producers and consumers block instead, and no item was lost or duplicated in any of the six configurations. Taking the mutex before the semaphore, however, deadlocks at once. The buffer capacity mattered far more than the number of threads: the same 400,000 items took 46.5 s with capacity 1 and 0.53 s with capacity 64. The condition variable showed that the flag and the signal are both needed: the flag records that the event happened and the signal wakes a sleeper, and the predicate must be re-checked in a loop. Blocking for one second cost 0.61 ms of CPU against 1000 ms for polling. For the very short waits of the bounded buffer, though, polling was much faster than blocking on this machine, so the cheaper method depends on how long a thread has to wait. The phase barrier made the multi-threaded grid match the sequential reference in every run, while the version without it was wrong in all 4096 cells and different every time. The barrier is also expensive, and each round lasts as long as its slowest thread. The lock-free stack passed its stress test with no lost or duplicated nodes and no TSan warnings, but it was not consistently faster than a mutex.
