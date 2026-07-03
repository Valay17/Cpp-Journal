#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

constexpr int kRunSeconds = 5;

/*
   Four synchronization modes, each run as its own separate process
   invocation, never combined in one run. Cache state, thread
   scheduling, and OS noise from one mode can otherwise bleed into
   the next and obscure the comparison.

   no-sync:   plain int, no atomics, no mutex. Deliberately racy and
              undefined behavior. Included only to show the floor, what
              throughput looks like when zero synchronization cost is
              paid. The final counter value will be wrong.

   mutex:     std::mutex protecting a plain int. One thread at a time,
              fully serialized. Safe, correct, and the baseline everyone
              already knows is slow under contention.

   lock-free: std::atomic<int> with compare_exchange_weak retry loop.
              At least one thread completes on every contention point,
              but a specific unlucky thread can retry an unbounded number
              of times. System always makes progress, individual threads
              are not guaranteed to.

   wait-free: std::atomic<int> with fetch_add. Every thread completes
              in exactly one step regardless of what other threads are
              doing. No retry, no starvation possible.

   Thread count is determined at runtime via hardware_concurrency(),
   so this benchmark scales to the actual machine it runs on rather
   than assuming a fixed core count.

   Each thread counts its own completions independently, not via a
   shared total, so the per-thread spread (min/max/average) is
   computable at the end. That spread is specifically what makes the
   lock-free vs wait-free distinction visible in numbers: wait-free
   threads should show tight, consistent per-thread counts, while
   lock-free threads under contention can show wide variance between
   lucky and unlucky threads.

   Second argument selects thread count: "max" uses
   hardware_concurrency(), or pass a specific number like "2" to run
   at low contention for comparison.
*/

int plain_counter = 0;
std::atomic<int> atomic_counter{0};
std::mutex mtx;

void run_no_sync(long long& local_count, std::atomic<bool>& stop) {
    while (!stop.load(std::memory_order_relaxed)) {
        plain_counter++;
        local_count++;
    }
}

void run_mutex(long long& local_count, std::atomic<bool>& stop) {
    while (!stop.load(std::memory_order_relaxed)) {
        std::lock_guard<std::mutex> lock(mtx);
        plain_counter++;
        local_count++;
    }
}

void run_lock_free(long long& local_count, std::atomic<bool>& stop) {
    while (!stop.load(std::memory_order_relaxed)) {
        int old = atomic_counter.load(std::memory_order_relaxed);
        while (!atomic_counter.compare_exchange_weak(
            old, old + 1, std::memory_order_relaxed));
        local_count++;
    }
}

void run_wait_free(long long& local_count, std::atomic<bool>& stop) {
    while (!stop.load(std::memory_order_relaxed)) {
        atomic_counter.fetch_add(1, std::memory_order_relaxed);
        local_count++;
    }
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " [no-sync|mutex|lock-free|wait-free] [max|<thread_count>]\n";
        return 1;
    }

    const char* mode = argv[1];
    unsigned int thread_count = 0;

    if (std::strcmp(argv[2], "max") == 0) {
        thread_count = std::thread::hardware_concurrency();
        if (thread_count == 0) thread_count = 4;
    } else {
        thread_count = static_cast<unsigned int>(std::atoi(argv[2]));
        if (thread_count == 0) {
            std::cout << "Invalid thread count\n";
            return 1;
        }
    }

    if (std::strcmp(mode, "no-sync")    != 0 &&
        std::strcmp(mode, "mutex")      != 0 &&
        std::strcmp(mode, "lock-free")  != 0 &&
        std::strcmp(mode, "wait-free")  != 0) {
        std::cout << "Usage: " << argv[0] << " [no-sync|mutex|lock-free|wait-free] [max|<thread_count>]\n";
        return 1;
    }

    std::vector<long long> local_counts(thread_count, 0);
    std::atomic<bool> stop{false};
    std::vector<std::thread> threads;

    for (unsigned int i = 0; i < thread_count; ++i) {
        if (std::strcmp(mode, "no-sync") == 0)
            threads.emplace_back(run_no_sync, std::ref(local_counts[i]), std::ref(stop));
        else if (std::strcmp(mode, "mutex") == 0)
            threads.emplace_back(run_mutex, std::ref(local_counts[i]), std::ref(stop));
        else if (std::strcmp(mode, "lock-free") == 0)
            threads.emplace_back(run_lock_free, std::ref(local_counts[i]), std::ref(stop));
        else
            threads.emplace_back(run_wait_free, std::ref(local_counts[i]), std::ref(stop));
    }

    std::this_thread::sleep_for(std::chrono::seconds(kRunSeconds));
    stop.store(true, std::memory_order_relaxed);

    for (auto& t : threads) t.join();

    long long total = 0;
    long long min_count = local_counts[0];
    long long max_count = local_counts[0];

    for (unsigned int i = 0; i < thread_count; ++i) {
        total += local_counts[i];
        min_count = std::min(min_count, local_counts[i]);
        max_count = std::max(max_count, local_counts[i]);
        std::cout << "thread " << i << ": " << local_counts[i] << " ops completed\n";
    }

    std::cout << "\nmode:              " << mode << "\n";
    std::cout << "threads:           " << thread_count << "\n";
    std::cout << "duration:          " << kRunSeconds << "s\n";
    std::cout << "total ops:         " << total << "\n";
    std::cout << "throughput:        " << total / kRunSeconds << " ops/sec\n";
    std::cout << "min (per thread):  " << min_count << " ops\n";
    std::cout << "max (per thread):  " << max_count << " ops\n";
    std::cout << "spread:            " << max_count - min_count << " ops ("
              << (min_count > 0 ? (max_count - min_count) * 100 / min_count : 0)
              << "% variance across threads)\n";

    return 0;
}
