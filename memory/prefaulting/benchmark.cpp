#include <iostream>
#include <chrono>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <string>

/*
get_minor_faults reads the process's own minor page fault count from the
kernel. A minor fault is what happens when a virtual address is already
reserved but has no physical page behind it yet, the kernel maps a
physical page in and lets execution continue, no disk I/O involved. This
is the fault type first-touch allocation triggers.
*/
long get_minor_faults() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_minflt;
}

int main(int argc, char** argv) {
    bool use_mlock = (argc > 1 && std::string(argv[1]) == "mlock");

    const size_t page_size = sysconf(_SC_PAGESIZE);
    const size_t size = 256UL * 1024 * 1024; // 256 MB
    char* buf = new char[size];

    if (use_mlock) {
        long f0 = get_minor_faults();
        auto s = std::chrono::steady_clock::now();
        mlock(buf, size);
        auto e = std::chrono::steady_clock::now();
        long f1 = get_minor_faults();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(e - s).count();
        std::cout << "mlock: " << us << " us, minor faults during mlock: " << (f1 - f0) << "\n";
    }

    long f_before_first = get_minor_faults();
    auto start1 = std::chrono::steady_clock::now();
    for (size_t i = 0; i < size; i += page_size)
        buf[i] = 1;
    auto end1 = std::chrono::steady_clock::now();
    long f_after_first = get_minor_faults();

    auto start2 = std::chrono::steady_clock::now();
    for (size_t i = 0; i < size; i += page_size)
        buf[i] = 2;
    auto end2 = std::chrono::steady_clock::now();
    long f_after_second = get_minor_faults();

    auto us1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1).count();
    auto us2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2).count();

    std::cout << "first touch:  " << us1 << " us, minor faults: " << (f_after_first - f_before_first) << "\n";
    std::cout << "second touch: " << us2 << " us, minor faults: " << (f_after_second - f_after_first) << "\n";

    if (use_mlock) munlock(buf, size);
    delete[] buf;
}
