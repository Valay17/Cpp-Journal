# False Sharing

Full writeup: Valay17.github.io/Portfolio/blog/cpu/false-sharing

Two threads writing to independent variables can still destroy each other's
performance if those variables share a CPU cache line. The CPU does not move
single bytes between memory and cache. It moves 64 byte cache lines. If two
atomics live on the same line, every write from one thread invalidates the
line for the other thread's core, forcing a reload from a shared cache level
or memory. No data race, no shared variable, just bad layout.

## What the code demonstrates

Two threads each incrementing their own `std::atomic<int>` in a tight loop.
One layout packs both counters on the same cache line. The other pads each
counter to its own 64 byte line with `alignas(64)`. Same logic, same atomics,
different layout. The two layouts are run as separate process invocations,
not back to back in the same run, so cache state and scheduling from one
version cannot bleed into the other.

## Key insight

```cpp
struct Unpadded {
    std::atomic<int> a;
    std::atomic<int> b; // likely same cache line as a
};

struct Padded {
    alignas(64) std::atomic<int> a; // own cache line
    alignas(64) std::atomic<int> b; // own cache line
};
```

The fix costs 56 bytes of padding per counter and nothing else. The logic is
identical. Only the memory layout changes.

## Run

Compile once:

```bash
g++ -O2 -std=c++20 -pthread -g benchmark.cpp -o benchmark
```
`-O2` is the standard optimization level for this kind of benchmark.
`-pthread` is required on Linux with GCC whenever `std::thread` is used.
`-g` keeps debug symbols so perf can show function names instead of raw
addresses, useful later even though this example has no separate
compilation units to misattribute.

Run each layout as its own process, separately:

```bash
./benchmark unpadded
./benchmark padded
```
Running these as two separate invocations, not in the same process one
after another, means each run starts with a cold cache and a freshly
scheduled set of threads. That keeps the comparison fair.

## Measuring cache misses with perf

Allow perf to read hardware counters without root:

```bash
sudo sysctl -w kernel.perf_event_paranoid=1
```
This kernel setting controls how much access non-root users have to
performance monitoring counters. The default on most distros blocks
or limits perf for regular users. Setting it to 1 allows process-level
counter access, which is enough for `perf stat` here without needing
to run perf itself as root.

Run perf against each binary invocation separately:

```bash
perf stat -e cache-misses,cache-references,L1-dcache-load-misses ./benchmark unpadded
perf stat -e cache-misses,cache-references,L1-dcache-load-misses ./benchmark padded
```
`cache-misses` and `cache-references` give the overall miss rate.
`L1-dcache-load-misses` isolates misses at the L1 data cache specifically,
which is where false sharing first shows up before it ever reaches L2 or
L3.

## Output

```
$ ./benchmark unpadded
Unpadded (likely false sharing): 3304 ms
a=200000000 b=200000000

$ ./benchmark padded
Padded (own cache line each):    814 ms
a=200000000 b=200000000
```

Padding the two counters onto separate cache lines cuts wall clock time
by roughly 4x on this run, from 3.3 seconds down to 0.8 seconds, for
identical logic.

## perf stat

```
$ perf stat -e cache-misses,cache-references,L1-dcache-load-misses ./benchmark unpadded
Unpadded (likely false sharing): 3288 ms
a=200000000 b=200000000

 Performance counter stats for './benchmark unpadded':

        45,782,528      cache-misses
        49,254,219      cache-references
        45,635,715      L1-dcache-load-misses

       3.292390751 seconds time elapsed
       6.576136000 seconds user
       0.004000000 seconds sys


$ perf stat -e cache-misses,cache-references,L1-dcache-load-misses ./benchmark padded
Padded (own cache line each):    813 ms
a=200000000 b=200000000

 Performance counter stats for './benchmark padded':

           213,758      cache-misses
         1,161,304      cache-references
           147,277      L1-dcache-load-misses

       0.817276949 seconds time elapsed
       1.625410000 seconds user
       0.004001000 seconds sys
```

The cache miss counts tell the real story. The unpadded version has a
cache-references count almost identical to its L1-dcache-load-misses count,
meaning nearly every single cache access misses. That is what false sharing
looks like at the hardware level: two cores fighting over ownership of the
same line, so neither one's data ever stays cached. The padded version drops
L1-dcache-load-misses by roughly 300x, from 45.6 million down to 147 thousand,
which is a far larger swing than the 4x wall clock difference suggests on its
own. The wall clock number is the visible symptom. The cache miss count is
the root cause.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 14.3.0