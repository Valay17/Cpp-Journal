#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

constexpr long long kIterations = 200'000'000;

// Both counters likely land on the same 64 byte cache line.
// Every write from one thread invalidates the line for the other core.
struct Unpadded {
    std::atomic<int> a{0};
    std::atomic<int> b{0};
};

// Each counter gets its own cache line. No invalidation traffic
// between the two threads, same logic otherwise.
struct Padded {
    alignas(64) std::atomic<int> a{0};
    alignas(64) std::atomic<int> b{0};
};

template <typename Counters>
long long run(Counters& c) {
    auto worker_a = [&]() {
        for (long long i = 0; i < kIterations; ++i) {
            c.a.fetch_add(1, std::memory_order_relaxed);
        }
    };
    auto worker_b = [&]() {
        for (long long i = 0; i < kIterations; ++i) {
            c.b.fetch_add(1, std::memory_order_relaxed);
        }
    };

    auto start = std::chrono::steady_clock::now();

    std::thread t1(worker_a);
    std::thread t2(worker_b);
    t1.join();
    t2.join();

    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

int main(int argc, char** argv) {
    if (argc != 2 || (std::strcmp(argv[1], "padded") != 0 && std::strcmp(argv[1], "unpadded") != 0)) {
        std::printf("Usage: %s [padded|unpadded]\n", argv[0]);
        return 1;
    }

    // Each mode runs in its own process invocation. Running both
    // versions back to back in one process would let cache state,
    // thread scheduling, and frequency scaling from the first run
    // bleed into the second, which defeats the point of the comparison.
    if (std::strcmp(argv[1], "unpadded") == 0) {
        Unpadded c;
        long long ms = run(c);
        std::printf("Unpadded (likely false sharing): %lld ms\n", ms);
        std::printf("a=%d b=%d\n", c.a.load(), c.b.load());
    } else {
        Padded c;
        long long ms = run(c);
        std::printf("Padded (own cache line each):    %lld ms\n", ms);
        std::printf("a=%d b=%d\n", c.a.load(), c.b.load());
    }

    return 0;
}
