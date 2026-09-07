# True Sharing: When the Cost Is Legitimate

Full writeup: https://valay17.github.io/Portfolio/blog/cpu/true-sharing

Not all cache line sharing costs you anything. Some of it is completely free, and the line between free and expensive comes down to one thing: whether anyone writes.

True sharing is when multiple threads need the same piece of data, not an accident of memory layout, an actual shared counter, a shared flag, something the threads are correctly coordinating around. When every thread involved is only reading that cache line, the cache coherence protocol lets each core hold its own copy simultaneously, no conflict, no invalidation, no cost beyond the first fetch. Multiple cores reading the same data at the same time is exactly what caching is supposed to make cheap.

The moment one thread writes to that cache line, everything changes. Every other core's copy has to be invalidated immediately, since it no longer reflects what's actually stored. The next core that touches that line has to fetch it fresh. If threads are writing to that shared line frequently, the line bounces from core to core, invalidated and refetched over and over, each trip costing cycles compared to a cache hit.

This is what makes true sharing fundamentally different from the accidental kind covered in the false sharing entry in this repo. When sharing is accidental, unrelated variables happening to land on the same cache line, the fix is straightforward: separate them, pad them apart, make sure they don't collide. True sharing doesn't have that option, the threads need that same piece of data, moving it apart isn't possible without changing what the program does. Fixing true sharing means reducing how often the write happens, restructuring the algorithm to batch updates instead of hitting the shared line constantly, or accepting the contention as the cost of coordinating threads that legitimately depend on shared state.

## A note on what this benchmark actually needs

This requires actual multiple physical cores to demonstrate anything. I am using a single CPU machine with mutliple cores and on a single core every thread here just gets time-sliced on that one core. There is no cross-core cache-coherence traffic to measure in that situation at all, whatever numbers come out reflect thread scheduling overhead and the base instruction cost difference between an atomic load and a `fetch_add`, not the mechanism this entry is about. The code itself is correct either way, this is purely a statement about what hardware is needed to get a meaningful result out of running it. A multi-core machine, which is what this repo's own environment block below describes, is required for the numbers in the Output section to mean anything.

## What the code demonstrates

One file, two access patterns, thread count selectable on the command line. `reader_work` only ever loads the shared atomic, every thread reading, no writes anywhere. `writer_work` calls `fetch_add`, a read-modify-write, on every iteration. Run each mode at increasing thread counts, on actual multiple cores, and the reading version should scale far more gracefully than the writing version, which should show contention getting worse as more threads compete to invalidate and refetch the same line.

## Key insight

```cpp
std::atomic<long> shared_value{1};

// every thread only reads: no invalidation, cores share the cache line freely
long reader() {
    return shared_value.load();
}

// every thread writes: every write invalidates every other core's copy
void writer() {
    shared_value.fetch_add(1);
}
```

Same variable, same cache line, completely different cost profile depending only on whether the access is a read or a write.

## Run: benchmark.cpp

```bash
g++ -O2 -std=c++26 -pthread benchmark.cpp -o benchmark
./benchmark read 1
./benchmark read 2
./benchmark read 4
./benchmark read 6
./benchmark read 8
./benchmark read 12
./benchmark read 16
./benchmark read 32
./benchmark read 64
./benchmark write 1
./benchmark write 2
./benchmark write 4
./benchmark write 6
./benchmark write 8
./benchmark write 12
./benchmark write 16
./benchmark write 32
./benchmark write 64
```
`-pthread` is required on Linux with GCC for `std::thread` to link correctly. This range deliberately crosses the physical core count, 8 cores, 16 hardware threads with SMT on this CPU, so the low end isolates cache-coherence cost cleanly while the high end also picks up oversubscription overhead layered on top, worth keeping separate when reading the results below. Expect `read` to stay close to flat until thread count exceeds what the hardware can run concurrently, while `write` should climb sharply from the very first additional thread, well before oversubscription becomes a factor at all.

## Output

```
$ ./benchmark read 1
read with 1 threads: 4 ms
$ ./benchmark read 2
read with 2 threads: 5 ms
$ ./benchmark read 4
read with 4 threads: 6 ms
$ ./benchmark read 6
read with 6 threads: 6 ms
$ ./benchmark read 8
read with 8 threads: 5 ms
$ ./benchmark read 12
read with 12 threads: 11 ms
$ ./benchmark read 16
read with 16 threads: 12 ms
$ ./benchmark read 32
read with 32 threads: 29 ms
$ ./benchmark read 64
read with 64 threads: 47 ms

$ ./benchmark write 1
write with 1 threads: 82 ms
$ ./benchmark write 2
write with 2 threads: 326 ms
$ ./benchmark write 4
write with 4 threads: 794 ms
$ ./benchmark write 6
write with 6 threads: 1215 ms
$ ./benchmark write 8
write with 8 threads: 1470 ms
$ ./benchmark write 12
write with 12 threads: 2084 ms
$ ./benchmark write 16
write with 16 threads: 2584 ms
$ ./benchmark write 32
write with 32 threads: 5050 ms
$ ./benchmark write 64
write with 64 threads: 10106 ms
```
Read stays flat from 1 to 8 threads, 4-6 ms regardless of thread count, since the Ryzen 7 5700U has 8 physical cores (16 threads via SMT), every reader up to that point gets its own hardware thread with nothing to contend over. It starts climbing past 12 threads, but that's oversubscription overhead, more software threads than hardware threads to run them on, not cache coherence traffic, worth being precise about rather than crediting it to the same mechanism.

Write never stops climbing, and climbs sharply from the very first added thread, 82 ms at one writer to 326 ms at two, roughly a 4x jump from a single additional thread with nothing but the cache coherence protocol to blame, since there's no oversubscription happening yet at that count. The gap between read and write at matching thread counts stays enormous throughout, roughly 20x at one thread, growing past 200x at eight, still above 200x at sixty-four, the cost of a shared line under write contention scales with contention itself, not with how much actual work each thread is doing.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 16.1.0
