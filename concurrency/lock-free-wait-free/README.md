# Lock-Free vs Wait-Free

Full writeup: https://valay17.github.io/Portfolio/blog/concurrency/lock-free-wait-free

Lock-free does not mean every thread makes progress. It means the system as a whole always does. Wait-free is the stronger guarantee: every single thread completes in a bounded number of steps, no thread can be starved no matter what the others are doing. Wait-free is always lock-free. Lock-free is not always wait-free. The lock in lock-free refers to software primitives, mutexes, spinlocks, system-wide synchronization, not hardware-level cache coherence. The LOCK prefix visible in the assembly below is a cache-line level operation, a fundamentally different mechanism from std::mutex or any kernel-level lock primitive.

## What the code demonstrates

`benchmark.cpp` runs four synchronization modes across all hardware threads the system reports, for 5 seconds each, as separate process invocations. Each thread counts its own completions independently rather than contributing to a shared total, so per-thread throughput (ops completed in the 5 second window) is visible alongside total throughput. This is not a direct latency measurement per operation, timing individual atomic increments at nanosecond granularity would require a different tool since steady_clock overhead per call would dwarf the measurement itself. What the per-thread spread actually shows is differential throughput under contention: a thread that kept winning the CAS race will show a higher local count than one that kept retrying, which implies higher effective throughput for that thread over the run. That spread is what separates lock-free from wait-free in the numbers: wait-free threads show tight, consistent per-thread counts since every thread completes in one step, while lock-free threads under contention show wider variance between lucky and unlucky threads.

`codegen.cpp` is not run. It is compiled to assembly and inspected with objdump, confirming what the two guarantees actually look like at the instruction level: a single `lock add` for wait-free, and a `lock cmpxchg` with a conditional jump back forming the retry loop for lock-free.

## Key insight

```cpp
// wait-free: one instruction, every thread done in one step
counter.fetch_add(1, std::memory_order_relaxed);

// lock-free: CAS loop, a thread can retry an unbounded number of times
int old = counter.load(std::memory_order_relaxed);
while (!counter.compare_exchange_weak(old, old + 1, std::memory_order_relaxed));
```

The LOCK prefix on both `lock add` and `lock cmpxchg` is cache-line level serialization, not a software mutex. It is a real cost, just a much cheaper and more localized one than anything involving the kernel or a system-wide lock.

## Run: benchmark.cpp

```bash
g++ -O2 -std=c++20 -pthread benchmark.cpp -o benchmark
```
`-O2` is the standard optimization level used across this repo. `-pthread` is required on Linux with GCC whenever `std::thread` is used.

Run each mode as its own process, at both thread counts, separately:

```bash
./benchmark no-sync max
./benchmark mutex max
./benchmark lock-free max
./benchmark wait-free max
./benchmark no-sync 2
./benchmark mutex 2
./benchmark lock-free 2
./benchmark wait-free 2
```
Running each mode as a separate invocation keeps cache state, thread scheduling, and OS noise from one mode from bleeding into the next. The `max` argument uses `hardware_concurrency()` to match the actual machine, `2` gives a low-contention comparison showing how the modes converge when threads rarely collide.

For hardware counter data on each mode:

```bash
sudo sysctl -w kernel.perf_event_paranoid=1
```
This kernel setting controls how much access non-root users have to performance monitoring counters. The default on most distros restricts perf for regular users. Setting it to 1 allows process-level counter access without running perf as root. Revert after this session:

```bash
sudo sysctl -w kernel.perf_event_paranoid=4
```

```bash
perf stat -e cycles,instructions,cache-misses ./benchmark no-sync max
perf stat -e cycles,instructions,cache-misses ./benchmark mutex max
perf stat -e cycles,instructions,cache-misses ./benchmark lock-free max
perf stat -e cycles,instructions,cache-misses ./benchmark wait-free max
```
`cycles` and `instructions` together give instructions-per-cycle across the full run. `cache-misses` shows the cache coherence traffic each mode generates, but the relationship between synchronization mechanism and cache miss count is not as straightforward as it might seem, see the analysis section below after the numbers for what the actual results show and why they contradict the intuitive ordering.

## Run: codegen.cpp

```bash
g++ -O2 -std=c++20 -c codegen.cpp -o codegen.o
objdump -d -M intel --no-show-raw-insn codegen.o | grep -A 10 "wait_free_increment\|lock_free_increment"
```
`-c` compiles to an object file without linking, since this file has no main and is not meant to run. `-M intel` selects Intel syntax. `--no-show-raw-insn` hides the raw instruction bytes. The `grep` keeps only the two relevant functions and the instructions immediately following each label.

## Godbolt output

Compiled at `-O2`, x86-64 gcc, confirms the instruction-level difference directly:

```asm
wait_free_increment():
        lock add        DWORD PTR counter[rip], 1   ; one instruction, always completes = wait-free
        ret                                          ; LOCK prefix = cache-line level, not a mutex

lock_free_increment():
        mov     eax, DWORD PTR counter[rip]
.L4:
        lea     edx, [rax+1]
        lock cmpxchg    DWORD PTR counter[rip], edx  ; atomic compare-and-swap
        jne     .L4                                   ; retry if another thread won the race
        ret                                           ; jne is what makes this lock-free not wait-free
```

## Output

```
$ ./benchmark no-sync max
thread 0: 19466665 ops completed
thread 1: 19807485 ops completed
thread 2: 68945545 ops completed
thread 3: 19137804 ops completed
thread 4: 17217137 ops completed
thread 5: 18964188 ops completed
thread 6: 18410261 ops completed
thread 7: 17148059 ops completed
thread 8: 17280055 ops completed
thread 9: 18666756 ops completed
thread 10: 17364072 ops completed
thread 11: 18403150 ops completed
thread 12: 24393692 ops completed
thread 13: 38802918 ops completed
thread 14: 21340647 ops completed
thread 15: 21248195 ops completed

mode:              no-sync
threads:           16
duration:          5s
total ops:         376596629
throughput:        75319325 ops/sec
min (per thread):  17148059 ops
max (per thread):  68945545 ops
spread:            51797486 ops (302% variance across threads)

$ ./benchmark mutex max
thread 0: 5255946 ops completed
thread 1: 5313175 ops completed
thread 2: 5248988 ops completed
thread 3: 5257324 ops completed
thread 4: 5308645 ops completed
thread 5: 5315688 ops completed
thread 6: 5322108 ops completed
thread 7: 5340459 ops completed
thread 8: 5268389 ops completed
thread 9: 5270105 ops completed
thread 10: 5270915 ops completed
thread 11: 5267563 ops completed
thread 12: 5105575 ops completed
thread 13: 5303270 ops completed
thread 14: 5191723 ops completed
thread 15: 5283641 ops completed

mode:              mutex
threads:           16
duration:          5s
total ops:         84323514
throughput:        16864702 ops/sec
min (per thread):  5105575 ops
max (per thread):  5340459 ops
spread:            234884 ops (4% variance across threads)

$ ./benchmark lock-free max
thread 0: 8286940 ops completed
thread 1: 5391855 ops completed
thread 2: 8139121 ops completed
thread 3: 5172837 ops completed
thread 4: 5811358 ops completed
thread 5: 7874923 ops completed
thread 6: 4435973 ops completed
thread 7: 8076835 ops completed
thread 8: 8020595 ops completed
thread 9: 7773630 ops completed
thread 10: 7979188 ops completed
thread 11: 4989614 ops completed
thread 12: 5015813 ops completed
thread 13: 4573590 ops completed
thread 14: 7734744 ops completed
thread 15: 5210213 ops completed

mode:              lock-free
threads:           16
duration:          5s
total ops:         104487229
throughput:        20897445 ops/sec
min (per thread):  4435973 ops
max (per thread):  8286940 ops
spread:            3850967 ops (86% variance across threads)

$ ./benchmark wait-free max
thread 0: 25581374 ops completed
thread 1: 25007731 ops completed
thread 2: 25458940 ops completed
thread 3: 26190119 ops completed
thread 4: 18647827 ops completed
thread 5: 17621426 ops completed
thread 6: 18620945 ops completed
thread 7: 17570521 ops completed
thread 8: 18206577 ops completed
thread 9: 18151006 ops completed
thread 10: 17930436 ops completed
thread 11: 17980958 ops completed
thread 12: 25566150 ops completed
thread 13: 25906503 ops completed
thread 14: 25554140 ops completed
thread 15: 24899374 ops completed

mode:              wait-free
threads:           16
duration:          5s
total ops:         348894027
throughput:        69778805 ops/sec
min (per thread):  17570521 ops
max (per thread):  26190119 ops
spread:            8619598 ops (49% variance across threads)

$ ./benchmark no-sync 2
thread 0: 703521160 ops completed
thread 1: 697131327 ops completed

mode:              no-sync
threads:           2
duration:          5s
total ops:         1400652487
throughput:        280130497 ops/sec
min (per thread):  697131327 ops
max (per thread):  703521160 ops
spread:            6389833 ops (0% variance across threads)

$ ./benchmark mutex 2
thread 0: 79419951 ops completed
thread 1: 71104089 ops completed

mode:              mutex
threads:           2
duration:          5s
total ops:         150524040
throughput:        30104808 ops/sec
min (per thread):  71104089 ops
max (per thread):  79419951 ops
spread:            8315862 ops (11% variance across threads)

$ ./benchmark lock-free 2
thread 0: 60723971 ops completed
thread 1: 63488343 ops completed

mode:              lock-free
threads:           2
duration:          5s
total ops:         124212314
throughput:        24842462 ops/sec
min (per thread):  60723971 ops
max (per thread):  63488343 ops
spread:            2764372 ops (4% variance across threads)

$ ./benchmark wait-free 2
thread 0: 157196165 ops completed
thread 1: 155270127 ops completed

mode:              wait-free
threads:           2
duration:          5s
total ops:         312466292
throughput:        62493258 ops/sec
min (per thread):  155270127 ops
max (per thread):  157196165 ops
spread:            1926038 ops (1% variance across threads)
```

## perf stat output

```
$ perf stat -e cycles,instructions,cache-misses ./benchmark no-sync max
mode:              no-sync
threads:           16
duration:          5s
total ops:         330156411
throughput:        66031282 ops/sec
min (per thread):  16593301 ops
max (per thread):  36308420 ops
spread:            19715119 ops (118% variance across threads)

 Performance counter stats for './benchmark no-sync max':

   318,838,755,763      cycles
     2,053,654,746      instructions
       537,398,518      cache-misses

       5.004326888 seconds time elapsed
      76.219222000 seconds user
       0.108388000 seconds sys

$ perf stat -e cycles,instructions,cache-misses ./benchmark wait-free max
mode:              wait-free
threads:           16
duration:          5s
total ops:         354995271
throughput:        70999054 ops/sec
min (per thread):  16939351 ops
max (per thread):  25901060 ops
spread:            8961709 ops (52% variance across threads)

 Performance counter stats for './benchmark wait-free max':

   327,808,472,853      cycles
     2,079,813,430      instructions
       629,703,439      cache-misses

       5.006693016 seconds time elapsed
      78.505311000 seconds user
       0.050837000 seconds sys

$ perf stat -e cycles,instructions,cache-misses ./benchmark lock-free max
mode:              lock-free
threads:           16
duration:          5s
total ops:         102793428
throughput:        20558685 ops/sec
min (per thread):  4300058 ops
max (per thread):  8919135 ops
spread:            4619077 ops (107% variance across threads)

 Performance counter stats for './benchmark lock-free max':

   318,757,724,598      cycles
     2,460,894,772      instructions
       156,744,782      cache-misses

       5.005698274 seconds time elapsed
      76.313461000 seconds user
       0.076569000 seconds sys

$ perf stat -e cycles,instructions,cache-misses ./benchmark mutex max
mode:              mutex
threads:           16
duration:          5s
total ops:         83204721
throughput:        16640944 ops/sec
min (per thread):  5104763 ops
max (per thread):  5282275 ops
spread:            177512 ops (3% variance across threads)

 Performance counter stats for './benchmark mutex max':

   202,200,501,614      cycles
    52,455,216,290      instructions
       435,727,936      cache-misses

       5.003902237 seconds time elapsed
       7.570596000 seconds user
      53.920382000 seconds sys
```

## What the numbers actually show

**Throughput at 16 threads**: wait-free leads at 70.7M ops/sec, no-sync comes second at 75.3M (no synchronization overhead at all, just racy writes), while mutex and lock-free are close together at 16.9M and 20.9M respectively. The near-identical throughput of mutex and lock-free under heavy contention contradicts the common assumption that lock-free is always faster than mutex. Under 16 threads hammering a single shared counter, the CAS retry loop wastes as many cycles on failed attempts as the mutex wastes on serialization. Lock-free's advantage shows up in different scenarios: lower contention, shorter critical sections, or latency-sensitive workloads where the mutex's kernel involvement on contention is the real bottleneck.

**Throughput at 2 threads**: mutex (30.1M) now beats lock-free (24.8M), and the gap between them is larger than at 16 threads. This is the opposite of what most people expect. At 2 threads, mutex contention is low enough that the kernel path is rarely hit, so mutex overhead reduces to a few atomic operations on the lock word itself, which turns out to be cheaper than even a lightly-contended CAS loop on this workload. wait-free at 62.5M ops/sec is again the clear winner at both thread counts. The key insight from comparing both runs is that lock-free never beats mutex on this specific benchmark regardless of thread count, because the workload (a single shared counter, no other work) maximizes the CAS retry overhead relative to what lock-free gains.

**Per-thread spread and what it actually measures**: lock-free drops from 86% spread at 16 threads to 4% at 2 threads. This directly proves the spread is contention-driven, not an inherent property of lock-free. At 2 threads there are few enough CAS collisions that neither thread gets significantly unlucky. Mutex holds 4% spread at 16 threads and 11% at 2 threads, where the slight increase is expected since with fewer threads serialization is less complete and one thread can occasionally hold the lock longer without others piling up. wait-free shows 49% spread at 16 threads and 1% at 2 threads. The 16-thread spread is entirely hardware topology (physical core groupings visible in lscpu output), not retry behavior, since fetch_add has no retry loop. At 2 threads on two logical cores of the same physical core, hardware asymmetry disappears and spread collapses to 1%.

**no-sync at 2 vs 16 threads**: 280M ops/sec at 2 threads versus 75.3M at 16 threads, nearly 4x more throughput with fewer threads. With 2 threads rarely hitting the same cache line at the same instant, the racy writes almost never overwrite each other and the spread is 0%. At 16 threads, the 302% spread and one thread getting 68M completions while others got 17M reflects how badly racy writes can skew when cache line ownership bounces between cores with no coordination.

**perf stat**: cache-misses tell the coherence story directly. Lock-free at 156M is the lowest by a wide margin, lower than mutex (435M) and far lower than no-sync (537M) and wait-free (629M). No-sync's high miss count shows that racy writes without a LOCK prefix still generate MESI invalidation traffic, the coordination cost is just moved from software to hardware with no atomicity to show for it. Wait-free's 629M misses are the direct cost of 354M successful LOCK-prefixed writes: every one of them forces an exclusive ownership round-trip. Lock-free's low count comes from the retry loop itself: failed CAS attempts often read a value already sitting in the thread's own cache before attempting another write, so not every retry generates fresh coherence traffic. The IPC numbers (no-sync 0.006, wait-free 0.006, lock-free 0.008, mutex 0.259) show all three atomic modes spending roughly one instruction per 160 cycles, the cache line bouncing penalty made visible in aggregate. Mutex's 0.259 IPC is anomalously high because most threads are sleeping rather than stalling, and its 53.9s sys time versus under 0.11s for every other mode is where its actual cost lives: kernel scheduling overhead, not hardware coherence.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 14.3.0
