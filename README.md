# C++ Journal

A growing collection of C++ internals, low-level systems, and performance
engineering topics. Every entry pairs a concept with runnable code and real
benchmark output from the target machine.

Each entry corresponds to a post published on LinkedIn. The repo is the
proof-of-work version: working examples, measured numbers, no hand-waving.

Status: updated daily.

---

## What "depth" means here

Most explanations of these topics stop at the conceptual level. Each entry
here includes at least one detail a surface-level explanation would miss:
the actual hardware mechanism behind a behavior, the specific standards-level
tool that addresses it, or the number that shows the cost instead of just
describing it.

---

## How entries are structured

Every topic lives in its own folder, named in lowercase with hyphens:

```
/<domain>/<topic-slug>/
    README.md
    benchmark.cpp       # timing-focused demonstration
    main.cpp            # standalone concept demo (only when it adds value)
```

One file is used when the benchmark itself is the clearest way to show the
concept. A second file is added only when a clean conceptual example and a
timing harness genuinely serve different purposes.

---

## Domains

| Folder | Covers |
|---|---|
| [language/](language/) | Object model, lifetime, value categories, ODR, copy/move semantics |
| [compiler/](compiler/) | Codegen, optimization passes, inlining, constant folding, reading assembly |
| [cpu/](cpu/) | Cache hierarchy, branch prediction, pipeline stalls, false sharing, prefetching |
| [memory/](memory/) | Allocators, alignment, NUMA, memory ordering, cache-line layout |
| [intrinsics/](intrinsics/) | SIMD, SSE/AVX/AVX-512, manual vectorization, throughput vs latency |
| [stl/](stl/) | Container internals, iterator invalidation, allocator-aware containers, SSO |
| [templates/](templates/) | SFINAE, concepts, CRTP, constexpr, compile-time cost |
| [performance/](performance/) | Profiling, flamegraphs, PGO, LTO, hot/cold splitting |
| [low-latency/](low-latency/) | Lock-free structures, RDTSC, kernel bypass, CPU pinning, OS jitter |
| [concurrency/](concurrency/) | Atomics, acquire-release, sequential consistency, data races |

Folders are created as entries are published. The table above is the map,
not a promise that every folder exists yet.

---

## Blog

Longer write-ups for topics that warrant more depth than a LinkedIn post
allows: [link here once live]
