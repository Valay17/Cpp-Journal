#include <sys/mman.h>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <chrono>
#include <random>
#include <vector>
#include <algorithm>
#include <string>

/*
This isolates the TLB (Translation Lookaside Buffer) effect specifically,
separate from the page fault cost covered in the prefaulting entry. Every
page is touched once, untimed, before the measured section starts, so the
timed loop never pays a page fault, only whatever cost comes from jumping
between pages the TLB may or may not still have cached.

Offsets are page-aligned and shuffled once up front, so the access order
itself is not part of what gets measured, only the act of touching each
page in a random rather than sequential order. Sequential access would let
the CPU's prefetcher and locality hide most of the TLB cost, random access
does not.

MADV_HUGEPAGE and MADV_NOHUGEPAGE ask the kernel's Transparent Huge Pages
mechanism to back this mapping with 2MB pages or keep it at the standard
4KB, respectively. This is a different mechanism from explicit hugetlbfs
reservation, THP is opportunistic and reclaimable, hugetlbfs requires
reserving a fixed pool upfront that nothing else on the system can use
until it is released.
*/

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

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "usage: ./benchmark [normal|huge]\n";
        return 1;
    }
    bool use_huge = (std::string(argv[1]) == "huge");

    const size_t page_size = 4096;
    const size_t size = 1UL * 1024 * 1024 * 1024; // 1 GB
    const size_t num_pages = size / page_size;

    void* buf = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (buf == MAP_FAILED) {
        std::cerr << "mmap failed\n";
        return 1;
    }

    madvise(buf, size, use_huge ? MADV_HUGEPAGE : MADV_NOHUGEPAGE);

    char* p = static_cast<char*>(buf);

    // fault every page in first, not timed, so the measured section below
    // only ever touches already-resident memory
    for (size_t i = 0; i < num_pages; i++) p[i * page_size] = 1;

    std::vector<size_t> offsets(num_pages);
    for (size_t i = 0; i < num_pages; i++) offsets[i] = i * page_size;
    std::mt19937 rng(42);
    std::shuffle(offsets.begin(), offsets.end(), rng);
    
    print_anon_huge_pages();
    
    const int passes = 20;
    volatile char sink = 0;
    auto start = std::chrono::steady_clock::now();
    for (int pass = 0; pass < passes; pass++) {
        for (size_t off : offsets) {
            sink += p[off];
        }
    }
    auto end = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << (use_huge ? "huge: " : "normal: ") << us << " us for "
              << (static_cast<size_t>(passes) * num_pages) << " random page touches\n";

    munmap(buf, size);
}
