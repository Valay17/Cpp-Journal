/*
   Not meant to be run. Compiled to assembly and inspected with objdump,
   to see the actual x86 instructions behind wait-free and lock-free
   atomic increments.

   wait_free_increment uses fetch_add, which maps to a single lock add
   instruction on x86. Every thread that calls this completes in exactly
   one step regardless of contention. That is the wait-free guarantee
   made visible in assembly: one instruction, no retry, no loop.

   lock_free_increment uses a CAS retry loop. compare_exchange_weak
   maps to lock cmpxchg, but the surrounding loop means a thread can
   fail and retry an unbounded number of times if contention is high
   enough. The system always makes progress (at least one thread wins
   each round), but no individual thread is guaranteed to complete in
   any fixed number of steps. That is lock-free without wait-free, and
   the jne instruction jumping back is what makes it visible directly
   in the assembly.

   The LOCK prefix on both instructions is a cache-line level operation,
   not a software mutex or a system-wide lock. It serializes access to
   that specific cache line across cores, which is real cost, but a
   fundamentally different mechanism from std::mutex or any kernel-level
   lock primitive.
*/

#include <atomic>

std::atomic<int> counter{0};

void wait_free_increment() {
    counter.fetch_add(1, std::memory_order_relaxed);
}

void lock_free_increment() {
    int old = counter.load(std::memory_order_relaxed);
    while (!counter.compare_exchange_weak(old, old + 1,
                            std::memory_order_relaxed));
}
