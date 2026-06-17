# C++ Journal

A living knowledge base of C++ internals, low-level systems, and performance engineering.

Every post published on LinkedIn has corresponding code here. The repo grows alongside the post cadence and serves as a proof-of-work archive for recruiters and engineers who find the profile.

---

## What this is

Each LinkedIn post covers one specific concept — a CPU quirk, a compiler behavior, a memory model subtlety, or a performance trap. The code here is the runnable, benchmarked version of whatever was posted. Nothing is fabricated: output placeholders are left blank and filled in after actually running the code on the target machine.

---

## Domains

| Folder | Topics |
|---|---|
| [language/](language/) | Core language mechanics, ODR, lifetime, value categories, copy/move semantics |
| [compiler/](compiler/) | Codegen, optimization passes, inlining, constant folding, reading assembly |
| [cpu/](cpu/) | Caches, branch prediction, pipeline stalls, false sharing, prefetching |
| [memory/](memory/) | Allocators, alignment, NUMA, memory ordering, cache-line layout |
| [intrinsics/](intrinsics/) | SIMD, SSE/AVX/AVX-512, manual vectorization, throughput vs latency |
| [stl/](stl/) | STL internals, iterator invalidation, allocator-aware containers, SBO |
| [templates/](templates/) | Template metaprogramming, SFINAE, concepts, CRTP, compile-time cost |
| [performance/](performance/) | Profiling, perf, flamegraphs, PGO, LTO, hot/cold splitting |
| [low-latency/](low-latency/) | Lock-free, wait-free, RDTSC, kernel bypass, CPU pinning, OS jitter |
| [concurrency/](concurrency/) | Atomics, acquire-release, sequential consistency, data race analysis |

---

## Folder structure

Each topic is a subdirectory under its domain, named in lowercase with hyphens:

```
/<domain>/<topic-slug>/
    README.md
    benchmark.cpp       # timing-focused demonstration
    main.cpp            # standalone concept demo (if needed separately)
```

One file is used when the benchmark is the demonstration. Two files are used when a clean conceptual example and a timing harness genuinely serve different purposes.

### Topic README format

1. One-paragraph explanation of the concept
2. What the code demonstrates
3. Key insight or fix (short code block if relevant)
4. Compile and run commands
5. Output placeholder (filled in after running)
6. perf stat placeholder (if applicable)
7. CPU / Kernel / Compiler fields (filled in after running)

---

## Build

Default compile command (Linux, GCC):

```bash
g++ -O2 -std=c++20 -pthread benchmark.cpp -o benchmark && ./benchmark
```

Flags used per topic type:

| Scenario | Extra flags |
|---|---|
| SIMD / intrinsics / auto-vectorization | `-march=native` |
| Unoptimized codegen comparison | `-O0` |
| perf with symbol names | `-g` |
| Flamegraphs | `-fno-omit-frame-pointer` |

---

## Profiling

Allow perf without root:

```bash
sudo sysctl -w kernel.perf_event_paranoid=1
```

Standard perf command:

```bash
perf stat -e cache-misses,cache-references,L1-dcache-load-misses ./benchmark
```

For branch prediction or IPC analysis, add:

```
-e cycles,instructions,branch-misses
```

---

## Timing notes

- `std::chrono` is the default for microsecond-range measurements.
- For nanosecond-range work, chrono resolution may not be sufficient. Options: `__rdtsc()` for cycle-accurate measurement (x86, note frequency scaling), Google Benchmark for statistical rigor, or nanobench as a lighter alternative.
- Any topic where chrono is genuinely inadequate calls it out in the topic README.

---

## Environment

- OS: Ubuntu / Kali Linux (Debian-based)
- Compiler: GCC, C++20
- Profiler: Linux perf
