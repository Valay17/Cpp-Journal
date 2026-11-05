#include <chrono>
#include <cstdio>
#include <ctime>
#include <thread>
#include <vector>
#include <atomic>

/*
Four threads call instance() at the same moment. The constructor spins for
300 ms, so the thread that runs it is busy the whole time while the others
have nothing to do but wait for it. Each thread records two durations
around its own call: wall time, how long the call took, and CPU time, how
long the thread was actually running during it. A thread that runs the
constructor shows 300 ms of both. A thread waiting on the guard shows 300
ms of wall time and almost no CPU time, because it is asleep in the
runtime library until the constructor finishes.

Fast is the same object with a constexpr constructor. It has no guard, so
a call has nothing to wait for.
*/

using clk = std::chrono::steady_clock;

static void spin_ms(int ms) {
    auto end = clk::now() + std::chrono::milliseconds(ms);
    while (clk::now() < end) {}
}

struct Slow {
    int value;
    Slow() : value(69) { spin_ms(300); }
};

Slow& slow_instance() {
    static Slow s;
    return s;
}

struct Fast {
    int value;
    constexpr Fast() : value(69) {}
};

Fast& fast_instance() {
    static Fast f;
    return f;
}

static double thread_cpu_ms() {
    timespec ts;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

template <class F>
void run(const char* name, F call) {
    constexpr int kThreads = 4;
    std::atomic<bool> go{false};
    double wall[kThreads], cpu[kThreads];
    std::vector<std::thread> threads;
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&, i] {
            while (!go.load()) {}
            auto w0 = clk::now();
            double c0 = thread_cpu_ms();
            call();
            cpu[i] = thread_cpu_ms() - c0;
            wall[i] = std::chrono::duration<double, std::milli>(clk::now() - w0).count();
        });
    }
    go.store(true);
    for (auto& t : threads) t.join();
    printf("%s\n", name);
    for (int i = 0; i < kThreads; ++i)
        printf("  thread %d: wall %7.2f ms, cpu %7.2f ms\n", i, wall[i], cpu[i]);
}

int main() {
    run("constructor that takes 300 ms (guarded):", [] { slow_instance(); });
    run("constexpr constructor (no guard):", [] { fast_instance(); });
}
