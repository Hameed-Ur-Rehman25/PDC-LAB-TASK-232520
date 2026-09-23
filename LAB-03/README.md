# Lab 03: Processes and Threads

**Name:** Hameed Ur Rehman &nbsp;|&nbsp; **Roll No:** 232520 &nbsp;|&nbsp; **Class:** BSCS VII C &nbsp;|&nbsp; **Course:** CS426L Introduction to Parallel & Distributed Computing

**Test machine:** Intel Core i7-1068NG7 @ 2.30 GHz, 8 logical cores, .NET SDK 9.0, all programs built in Release mode.
Raw console output from every run is in [output.txt](output.txt).

## Project Structure

All four tasks are in a single console project, [Lab03/](Lab03/), with one file per task:

| File | Task |
|---|---|
| [Program.cs](Lab03/Program.cs) | Entry point that picks which task to run |
| [Task1_ProcessLab.cs](Lab03/Task1_ProcessLab.cs) | Task 1: Process creation and address-space separation |
| [Task2_ArraySumThreads.cs](Lab03/Task2_ArraySumThreads.cs) | Task 2: Summing array slices across worker threads |
| [Task3_CreationOverhead.cs](Lab03/Task3_CreationOverhead.cs) | Task 3: Process- vs. thread-creation overhead |
| [Task4_ThreadLifecycle.cs](Lab03/Task4_ThreadLifecycle.cs) | Task 4: Thread lifecycle states |

Tasks 1 and 3 start this same executable as their child process. The `--child` argument runs Task 1's child branch, and `--noop` makes the child exit immediately for Task 3.

### How to Run

```bash
cd Lab03
dotnet run -c Release            # runs all four tasks in order
dotnet run -c Release -- 1       # run only Task 1 (use 2, 3 or 4 for the others)
```

---

## Task 1: Process Creation and Address-Space Separation

The parent starts a second copy of the same executable with `Process.Start()` and passes the `--child` argument. It then waits for the child with `WaitForExit()`. The executable path comes from `Environment.ProcessPath`.

### Results

| Role | Process ID (PID) | Final Counter Value |
|---|---|---|
| Parent | 19599 | 101 |
| Child | 19606 | 150 |

(Run 2 gave parent PID 19659 and child PID 19660, with the same counter values.)

### Discussion

**1. Were the PIDs different? What does this confirm?**
Yes. The parent was 19599 and the child was 19606. This confirms that `Process.Start()` asks the operating system to create a new, separate process with its own identity and resources. The child is not a thread or a function call inside the parent.

**2. Did either process's final counter affect the other's?**
No. The parent ended with 101 and the child ended with 150. Each process has its own private virtual address space, so each `counter` variable lives in different physical memory. Neither process can see or change the other's variable. Changes in one process stay inside that process.

**3. Does the child ever see the parent's in-progress value (101)?**
No. The child always starts from 100, because it runs `Main` again from the beginning and sets its own counter. With POSIX `fork()`, the child gets a copy-on-write copy of the parent's memory at the moment of the call. It would therefore start with `counter = 101` and share the parent's physical pages until one of them writes to the page. `Process.Start()` works like `fork()` followed by `exec()`. It loads a fresh program image, so the new address space starts with nothing from the parent. The only data passed across is what we send explicitly, such as the command-line argument `--child`.

---

## Task 2: Summing Array Slices Across Worker Threads

The program uses 10,000,000 `long` values (1, 2, ..., 10,000,000) split into 8 equal contiguous slices, one per logical core. Each worker writes its sum into its own `partialSums[idx]` slot. After `Join()`, the main thread adds up the partial sums.

### Results

| Array Size | Worker Threads Used | Threaded Total | Sequential Total | Match? (Y/N) |
|---|---|---|---|---|
| 10,000,000 | 8 | 50,000,005,000,000 | 50,000,005,000,000 | Y |

The expected result from the formula n(n+1)/2 = 10,000,000 × 10,000,001 / 2 = 50,000,005,000,000, so both totals are correct. The workers finish in a different order on each run, but the total is always the same.

### Discussion

**1. Why does writing to a separate slot avoid the need for a lock?**
No memory location is written by more than one thread, so there is no data race. If every thread did `total += x` on one shared `long`, each update would be a read-modify-write: load, add, then store. Two threads could load the same old value, and one thread's store would overwrite the other's, losing an update. That would need `lock` or `Interlocked.Add`. In this program, the main thread reads the slots only after `Join()`, and `Join()` also ensures that the workers' writes are visible to the main thread.

**2. How are leftover elements handled when the length does not divide evenly?**
This line gives the last worker everything from its start index to the end of the array:
```csharp
int end = (idx == numWorkers - 1) ? data.Length : start + sliceSize;
```
Integer division rounds `sliceSize` down, so the remaining `data.Length % numWorkers` elements go to the last worker instead of being dropped.

**3. How does this relate to Task 1?**
All 8 worker threads run inside a single process and share its address space. They read the same static `data` array and write into the same static `partialSums` array, with no copying or message passing. In Task 1 the situation was the opposite: the parent and child each had their own `counter`, and neither could see the other's memory. Sharing memory is what makes threads efficient here, and it is also why threads need rules such as "one slot per thread" or locks. Processes are isolated by default and do not need these rules.

---

## Task 3: Measuring Process- vs. Thread-Creation Overhead

The program runs 50 iterations of each kind after one warm-up round. For processes, it calls `Process.Start()` on this same Lab03 executable with the `--noop` argument, which makes it exit immediately, and then calls `WaitForExit()`. For threads, it runs `new Thread(() => { }).Start()` followed by `Join()`. Both loops are timed with `Stopwatch` in a Release build.

### Results

| Run | Avg. Process Creation (ms) | Avg. Thread Creation (ms) | Ratio (Process / Thread) |
|---|---|---|---|
| 1 | 50.633 | 0.096 | 525.5× |
| 2 | 75.733 | 0.179 | 424.1× |

### Discussion

**1. Which was more expensive, and by how much?**
Creating a process was far more expensive: about 50–76 ms per process, compared with about 0.1–0.18 ms per thread. That is roughly **420–525×**, between two and three orders of magnitude. Both averages varied between runs because of other activity on the machine, but the ratio stayed in the hundreds.

**2. Why does a process cost more than a thread?**
To create a process, the OS has to allocate a new virtual address space and page tables. It also has to create a new kernel process object, set up a new handle table and environment, and map and load the executable and its libraries. For a .NET program, the entire .NET runtime then has to start up again (GC heap, JIT, and assembly loading) before `Main` runs. Afterwards, all of this has to be torn down. A thread reuses everything the process already has: the address space, the loaded code, the heap, and open handles. The OS only allocates a stack and a small kernel scheduling structure, then adds the thread to the run queue.

**3. Would the ratio change with more substantial work?**
The ratio would **shrink** toward 1. The creation cost is roughly fixed, while useful work like summing a large array takes the same time whether a thread or a process does it. Once each unit of work takes much longer than about 50 ms, that work dominates both measurements and the ratio approaches 1. A process would still be somewhat slower if the data had to be copied or sent to it, because it cannot directly read the parent's array the way a thread can.

---

## Task 4: Thread Lifecycle States

### Results

| Point in Program | Thread.ThreadState Reported | Conceptual Lifecycle State |
|---|---|---|
| Immediately after creation, before Start() | `Unstarted` | New |
| Immediately after Start() | `Running` | Runnable / Ready (or Running) |
| While the worker is inside Thread.Sleep(200) | `WaitSleepJoin` | Blocked / Waiting |
| After Join() has returned | `Stopped` | Terminated |

.NET does not have a separate "Ready" flag. Once a thread has started, it reports `Running` whether it is actually running on a core or waiting in the OS run queue.

### Discussion

**1. Why is New → Runnable → Running cheaper for a thread than creating a process?**
For a thread, **New** is only a managed object. `Start()` asks the OS for a stack and a scheduling entry, which makes it **Runnable**. The scheduler can then move it to **Running** straight away, because its code, data, and runtime are already loaded in the process's address space. Task 3 measured this full path, including termination and `Join()`, at about 0.1–0.18 ms. A process has to go through address-space creation, loading the program image, and .NET runtime start-up before its first thread can even reach Running. That took about 50–76 ms, so a thread gets to useful work several hundred times sooner.

**2. Why are threads used for fine-grained parallelism?**
- *Creation overhead (Task 3):* When there are thousands of small tasks, a process's roughly 50 ms start-up cost would be larger than the work itself. Thousands of processes would add minutes of pure overhead, while threads cost fractions of a millisecond each. Thread pools reduce this further by reusing threads.
- *Address-space sharing (Task 2):* Threads can work directly on shared data, such as the same array, and combine results through memory. Processes would need to copy data in and send results back through pipes, files, or shared-memory setup.
- *Lifecycle speed (Task 4):* A thread moves from Unstarted to Running to Stopped almost immediately. Blocking (`WaitSleepJoin`) and waking up are cheap scheduler operations inside one process. This lets short units of work move through the lifecycle quickly, so more time goes to computation and less to setup.

**3. When is process isolation preferred despite its cost?**
A web browser runs each tab or site in its own process, and a server may run untrusted plug-ins or user code in separate worker processes. If code in one process crashes, corrupts memory, or is exploited by an attacker, the damage stays inside that process. The rest of the application keeps running, and the attacker cannot read other tabs' memory, such as passwords or cookies. Threads cannot offer this, because a single bad thread can crash or corrupt the whole shared address space. For long-lived, security-sensitive, or unreliable components, the one-time creation cost is worth the fault and security isolation.

---

## Conclusion

Process and thread behaved as the lab expected. `Process.Start()` created a separate process (different PID and independent counters). Threads shared one address space and correctly computed a parallel sum. Process creation was about 420–525× more expensive than thread creation on this machine, and `ThreadState` moved through Unstarted → Running → WaitSleepJoin → Stopped. Threads are the better choice for fine-grained parallel work, and processes are worth their cost when isolation is needed.
