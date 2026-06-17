# C++ Journal

A living knowledge base of C++ internals, low-level systems, and performance
engineering.

Every post published on LinkedIn has corresponding code here. The repo
grows daily. Clone any folder, build it, and run the benchmarks yourself.

Status: updated daily.


## What this is

Each LinkedIn post covers one specific concept: a CPU quirk, a compiler
behavior, a memory model subtlety, or a performance trap. The code here is
the runnable, benchmarked version of whatever was posted.



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
/<domain>/<topic>/
    README.md
    benchmark.cpp
    main.cpp
```

benchmark.cpp holds the example and timing.

main.cpp is added when the concept needs a standalone demo separate from the benchmark.

Each topic's README covers the concept, what the code shows, the compile and
run commands, and the actual output from running it.


## Finding a topic

Browse by domain using the table above. New folders are added as posts go
out, so a domain may be empty until its first topic lands. Topics post daily.
