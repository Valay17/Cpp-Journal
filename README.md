# C++ Journal

A living knowledge base of C++ internals, low-level systems, and performance engineering.

Every post published on LinkedIn has corresponding code here. The repo grows alongside the post cadence and serves as a reference for enthusiasts to find working examples and benchmarks that demonstrate the real-world impact of these techniques.


## What this is

Each LinkedIn post covers one specific concept — a CPU quirk, a compiler behavior, a memory model subtlety, or a performance trap. The code here is the runnable, benchmarked version of whatever was posted. Nothing is fabricated: output placeholders are left blank and filled in after actually running the code on the target machine.


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


## Folder structure

Each topic is a subdirectory under its domain, named in lowercase with hyphens:

```
/<domain>/<topic-slug>/
    README.md
    benchmark.cpp       # timing-focused demonstration
    main.cpp            # standalone concept demo (if needed separately)
```

One file is used when the benchmark is the demonstration. Two files are used when a clean conceptual example and a timing harness genuinely serve different purposes.
