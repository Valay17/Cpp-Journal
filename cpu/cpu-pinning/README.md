# CPU Pinning

Full writeup: https://valay17.github.io/Portfolio/blog/cpu/cpu-pinning

An OS thread that gets moved to a different core does not just change which CPU is running it, it loses everything that core had already warmed up for it. Every core has its own L1 and L2 cache. A thread running on one core for a while fills that cache with exactly the data it keeps touching, fast access, no trip to RAM. The moment the scheduler moves that thread to a different core, all of that is gone, the new core has cold caches for that thread's data, and the cost of rebuilding what the old core already had ready is paid in full cache miss latency.

CPU pinning exists entirely because of this. Pin a thread to a specific core and the scheduler is no longer allowed to move it based on whatever else is happening on the system. The cache stays warm because the thread never leaves. This matters more as core count goes up and contention for cores increases, since a thread getting preempted and rescheduled elsewhere is not free just because the new core happens to be idle, it is a trade of idle time for a cold cache, and depending on the workload, that trade can be worse.

## What the code demonstrates

`pin-to-core.cpp` is the pinning mechanism in isolation, a small helper that sets a thread's CPU affinity mask so the kernel cannot schedule it anywhere else, plus the shell equivalent for pinning an entire process with no code involved at all.

`migration-cost.cpp` forces the exact scenario the post describes, rather than waiting for the OS scheduler to maybe do it on a lightly loaded machine where migration might rarely happen at all. One mode pins a worker thread to one core for the entire run. The other mode starts pinned, then explicitly flips affinity between two specific cores at a fixed interval throughout the run, simulating a thread the scheduler keeps bouncing around. Both modes repeatedly sweep over the same fixed-size array, sized to fit in L2 but exceed L1 on this CPU, so the array benefits from staying resident in a core's cache across passes. The two cores used for migration must be separate physical cores, not two SMT threads of the same core sharing the same cache already, see the note in the code and below for how that was confirmed on this machine.

Run this with nothing else competing for the CPU. Background load on every core changes what the comparison actually measures, since the worker thread ends up competing for scheduling time on top of whatever cache effect migration causes on its own, which makes the two effects hard to tell apart in the resulting numbers.

## Confirming the core pair before running

Before running `migration-cost.cpp`, confirm which CPU numbers map to which physical cores on the machine actually running this, since the answer is not the same on every CPU:

```bash
lscpu -e
```
This lists every logical CPU with its CORE column. Two CPU numbers sharing the same CORE number are SMT threads of one physical core, sharing L1, L2, and usually L3 entirely, picking a pair like that as the migration target would defeat this demo, since there would be little or no cache to actually lose. Pick two CPU numbers with different CORE numbers instead. On this machine, CPU 0 (CORE 0) and CPU 4 (CORE 2) are confirmed separate physical cores and are the pair currently set in `migration-cost.cpp`. If running this on different hardware, re-run `lscpu -e` and update the `core_a`/`core_b` constants in the code to match a separate pair on that machine.

## Run: pin-to-core.cpp

```bash
g++ -O2 -std=c++20 -pthread pin-to-core.cpp -o pin-to-core && ./pin-to-core
```
`-O2` is the standard optimization level used across this repo. `-pthread` is required on Linux with GCC whenever `std::thread` is used.

## Run: migration-cost.cpp

```bash
g++ -O2 -std=c++20 -pthread migration-cost.cpp -o migration-cost
```

Run each mode as its own process, separately:

```bash
./migration-cost pinned
./migration-cost migrating
```
Running these separately, rather than back to back in one process, keeps any state from one run from influencing the other, the same separation used for every other good versus bad comparison in this repo.

For a closer look at cache behavior specifically:

```bash
sudo sysctl -w kernel.perf_event_paranoid=1
```
This kernel setting controls how much access non-root users have to performance monitoring counters. The default on most distros blocks or limits perf for regular users. Setting it to 1 allows process-level counter access without needing to run perf itself as root. 

This setting persists at the OS level beyond this one process, revert it after this session:

```bash
sudo sysctl -w kernel.perf_event_paranoid=4
```
4 is the typical distro default that restricts perf access again, check what the value was before changing it if a different default is expected on this machine.

```bash
perf stat -e cache-misses,cache-references,L1-dcache-load-misses,task-clock,cpu-migrations ./migration-cost pinned
perf stat -e cache-misses,cache-references,L1-dcache-load-misses,task-clock,cpu-migrations ./migration-cost migrating
```
`cache-misses` and `cache-references` give the overall miss rate, expected to be meaningfully higher for the migrating run. `L1-dcache-load-misses` isolates misses at the L1 level specifically, where the cost of a cold cache after migration first shows up, though on a working set sized to exceed L1 the way this one is, the bulk of the cost is expected to land above L1 rather than at it. `task-clock` confirms how much of the wall clock time was actually spent running rather than waiting to be scheduled, useful for ruling out scheduling overhead as the explanation for any timing difference. `cpu-migrations` confirms how many times the kernel actually moved the thread, useful for checking the forced affinity flips landed as migrations rather than just an affinity mask change with no measurable effect.

## Output

```
pinned: 132 ms, checksum = 380209871192064
migrating: 194 ms, checksum = 380209871192064
```

## perf stat output

```
 Performance counter stats for './migration-cost pinned':

            87,445      cache-misses
        17,203,800      cache-references
        16,529,335      L1-dcache-load-misses
            134.23 msec task-clock
                  2      cpu-migrations

       0.135797385 seconds time elapsed

       0.133893000 seconds user
       0.001998000 seconds sys

 Performance counter stats for './migration-cost migrating':

           423,157      cache-misses
        18,444,980      cache-references
        16,592,846      L1-dcache-load-misses
            193.12 msec task-clock
                 80      cpu-migrations

       0.197597735 seconds time elapsed

       0.193030000 seconds user
       0.004021000 seconds sys
```

`cache-misses` lands roughly 4.8 times higher for the migrating run while `cache-references` stayed close between the two, meaning the miss rate itself rose under migration rather than just the total number of accesses. `L1-dcache-load-misses` stayed flat between the two runs, consistent with the working set being sized to exceed L1, the cost here is landing at L2, not L1. `task-clock` tracks elapsed time closely in both runs, ruling out scheduling overhead as the explanation. `cpu-migrations` reads 2 for pinned (the initial placement) against 80 for migrating, matching `kPasses / kMigrationIntervalPasses` exactly and confirming the forced affinity flips landed as  kernel-level migrations.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 14.3.0
