# Lazy Allocation and the Cost of the First Touch

Full writeup: https://valay17.github.io/Portfolio/blog/memory/prefaulting

Calling `new` doesn't give you memory. It gives you a promise of memory.

When you allocate, the OS reserves a range of virtual addresses, but no physical page is behind any of it yet. The first time you touch that memory, write to it, read from it, anything, that access triggers a page fault. The kernel steps in, finds a physical page, zeroes it, updates the page table, then lets the instruction continue. All of this happens invisibly, inside what looks like a normal memory access in the source.

This is lazy allocation. It exists because most programs allocate more than they end up using, so mapping every page upfront would waste physical memory on pages nobody ever touches. The tradeoff is that the cost of "allocating" memory doesn't show up at the `new` call, it shows up later, scattered across whichever lines of code happen to touch that memory for the first time. For most code this is invisible and fine. For a latency-critical path, the first write to a freshly allocated buffer can stall for a page fault exactly where it can least be afforded, with nothing in the source marking that line as different from the one before it.

## What the code demonstrates

One file, two runs. The default run touches a freshly allocated buffer twice, once per page, and counts minor page faults via `getrusage` alongside timing each pass. The first pass faults in every page and is measured to be dramatically slower. The second touches the same addresses again, no faults, and the timing collapses accordingly.

The `mlock` mode shows the fix directly: locking the buffer with `mlock` forces every page to be resident before the call returns, moving all the fault cost into the `mlock` call itself, outside whatever comes after it in the hot path.

## Key insight

```cpp
char* buf = new char[size];

// first touch: each page faults in, kernel maps physical memory here
for (size_t i = 0; i < size; i += page_size)
    buf[i] = 1;

// second touch: pages already mapped, no fault, just a write
for (size_t i = 0; i < size; i += page_size)
    buf[i] = 2;

delete[] buf;
```

Both loops touch the same addresses. Only the first one pays for the page fault, since the second finds every page already mapped.

## Run: default (no mlock)

```bash
g++ -O2 -std=c++20 benchmark.cpp -o benchmark
./benchmark
```
No special flags needed. `getrusage` and `mlock` are both standard POSIX facilities. `page_size` comes from `sysconf(_SC_PAGESIZE)` rather than being hardcoded, since it should reflect whatever the running system actually uses, typically 4096 bytes on x86-64 Linux.

## Run: mlock mode

```bash
g++ -O2 -std=c++20 benchmark.cpp -o benchmark
./benchmark mlock
```
This locks the buffer with `mlock` before either touch loop runs. Expect the fault count to show up entirely during the `mlock` call, and both touch loops afterward to run at the same fast, fault-free speed the plain run's second loop gets.

## Output

```
$ ./benchmark
first touch:  121281 us, minor faults: 65537
second touch: 1015 us, minor faults: 0

$ ./benchmark mlock
mlock: 115069 us, minor faults during mlock: 65538
first touch:  1071 us, minor faults: 0
second touch: 877 us, minor faults: 0
```
Plain run: ~120x slower on first touch, fault count lands almost exactly on the predicted 65536 pages for a 256 MB buffer at 4096 bytes per page. `mlock` run: the fault cost moves entirely into the `mlock` call, both touch loops afterward are fast and fault-free, indistinguishable from the plain run's already-warm second touch.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 16.1.0
