# Hugepages and the TLB

Full writeup: https://valay17.github.io/Portfolio/blog/memory/hugepages

The TLB (Translation Lookaside Buffer) caches virtual-to-physical address translations. It has a fixed number of entries, each covering one page. At the default 4KB page size, even a large STLB of ~1536 entries only covers ~6MB of memory. Any working set larger than that causes TLB misses, each of which requires the hardware to walk the four-level page table, potentially four separate cache misses per miss. Hugepages (2MB) let one TLB entry cover 512 times more memory, fitting much larger working sets within TLB capacity and shrinking the page table walk from four levels to three.

## What the code demonstrates

One file, two modes. Both allocate a 1GB buffer and touch every page once outside the timed section, so page fault cost never contaminates the measurement. The timed section makes 20 passes over the buffer in a shuffled random order, touching one byte per page. Sequential access would let the hardware prefetcher hide most of the TLB miss cost, so the shuffled order specifically isolates translation latency.

`normal` mode disables THP with `MADV_NOHUGEPAGE`, ensuring 4KB pages. `huge` mode requests THP with `MADV_HUGEPAGE`. The benchmark reads `/proc/self/smaps` internally before each timed run and prints `AnonHugePages` to confirm whether the kernel actually honored the request, since `madvise` is a hint and the kernel can fall back to 4KB pages if contiguous physical memory is not available.

## Key insight

```cpp
// allocate 1GB anonymously
void* buf = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

// request 2MB hugepages for this mapping (THP, opportunistic)
madvise(buf, size, MADV_HUGEPAGE);

// touch every byte to fault pages in before the timed section
memset(buf, 1, size);

// ... timed random-access passes ...

// release: must use munmap, not free(), since mmap allocated this
munmap(buf, size);
```

`madvise` is a hint, not a command. `munmap` is the correct release for `mmap`-allocated memory. Using `free()` on a pointer from `mmap` is undefined behavior.

The `AnonHugePages` check is done inside the process via `/proc/self/smaps`:

```cpp
void print_anon_huge_pages() {
    std::ifstream smaps("/proc/self/smaps");
    if (!smaps) return;
    std::string line;
    long total = 0;
    while (std::getline(smaps, line)) {
        if (line.rfind("AnonHugePages:", 0) == 0) {
            long val = 0;
            std::sscanf(line.c_str(), "AnonHugePages: %ld kB", &val);
            total += val;
        }
    }
    std::cout << "AnonHugePages: " << total << " kB\n";
}
```

Called after `memset` and before the timed section in each mode. This avoids any shell timing race since the check and the benchmark run in the same process invocation.

## Run: benchmark

```bash
g++ -O2 -std=c++20 benchmark.cpp -o benchmark
./benchmark normal
./benchmark huge
```

`-O2` is the standard optimization level used across this repo. Run each mode as a separate invocation so neither result is biased by TLB or page cache state left from the other.

## Run: confirm THP status

```bash
cat /sys/kernel/mm/transparent_hugepage/enabled
```

The active mode is shown in brackets. `madvise` means THP applies only when a mapping explicitly requests it. If this shows `[never]`, the `huge` mode will silently behave identically to `normal` since the kernel ignores `MADV_HUGEPAGE` system-wide.

## Output

```
$ ./benchmark normal
AnonHugePages: 0 kB
normal: 78378 us for 5242880 random page touches

$ ./benchmark huge
AnonHugePages: 1048576 kB
huge: 56741 us for 5242880 random page touches

$ cat /sys/kernel/mm/transparent_hugepage/enabled
always [madvise] never
```

`AnonHugePages: 0 kB` confirms `MADV_NOHUGEPAGE` worked and the buffer is backed by standard 4KB pages. `AnonHugePages: 1048576 kB` confirms the full 1GB buffer is backed by 2MB hugepages (1048576 kB = 1GB). 78378 vs 56741 us, roughly 1.38x faster. `normal` has 262144 distinct 4KB pages, far exceeding STLB capacity, so nearly every random access is a TLB miss. `huge` has 512 distinct 2MB pages, which fits entirely within the STLB.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 16.1.0
