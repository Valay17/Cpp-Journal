#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <string>

/*
Same atomic, two access patterns. reader_work only ever loads shared_value,
every core involved can hold it in the cache-coherence protocol's Shared
state simultaneously, no invalidation traffic between them regardless of
thread count. writer_work calls fetch_add, a read-modify-write, so every
single call invalidates every other core's copy of that cache line. With
enough threads doing this at once, the line bounces from core to core
continuously.

This needs actual multiple physical cores to show anything meaningful. On
a single-core machine, or a container restricted to one core, every
thread just gets time-sliced on the same core, and there is no cross-core
invalidation traffic to measure at all, whatever numbers come out reflect
scheduling overhead and the base cost difference between an atomic load
and a fetch_add, not the mechanism this file exists to demonstrate.
*/

std::atomic<long> shared_value{1};

void reader_work(long iterations, long& out) {
    long sum = 0;
    for (long i = 0; i < iterations; i++) {
        sum += shared_value.load();
    }
    out = sum;
}

void writer_work(long iterations) {
    for (long i = 0; i < iterations; i++) {
        shared_value.fetch_add(1);
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "usage: ./benchmark [read|write] <thread_count>\n";
        return 1;
    }
    std::string mode = argv[1];
    int num_threads = std::stoi(argv[2]);
    const long iterations = 20'000'000;

    std::vector<std::thread> threads;
    std::vector<long> results(num_threads);

    auto start = std::chrono::steady_clock::now();
    for (int t = 0; t < num_threads; t++) {
        if (mode == "read") {
            threads.emplace_back(reader_work, iterations, std::ref(results[t]));
        } else {
            threads.emplace_back(writer_work, iterations);
        }
    }
    for (auto& th : threads) th.join();
    auto end = std::chrono::steady_clock::now();

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << mode << " with " << num_threads << " threads: " << ms << " ms\n";
}
