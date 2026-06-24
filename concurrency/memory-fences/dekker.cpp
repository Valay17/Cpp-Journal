#include <atomic>
#include <cstring>
#include <iostream>
#include <thread>

constexpr long long kIterations = 2'000'000;

/*
   Dekker-style mutual exclusion, deliberately broken.

   Two threads each set their own plain (non-atomic) flag, then check the
   other thread's flag before entering a critical section. If both threads
   see the other's flag as still false, both believe they have exclusive
   access and both enter the critical section at the same time. That is
   the violation this file counts.

   flag_a and flag_b are plain bool, not atomic. Nothing here forbids the
   compiler or CPU from reordering "set my flag" after "read your flag",
   which is exactly the reordering that causes the violation. A standalone
   atomic_thread_fence placed between the two operations prevents that
   reordering, but only because a dummy atomic operation is also present.
   A fence with no atomic anywhere in the picture has no effect, which is
   why dummy_atomic exists below and why both threads touch it.

   Both threads increment a shared counter inside the critical section
   without any protection. If the violation never happens, the counter
   simply ends up wrong by some amount, which is itself further evidence
   nothing actually serialized the two threads. The "both entered at once"
   count is the direct measurement, the wrong final counter value is the
   supporting one.
*/

bool flag_a = false;
bool flag_b = false;
std::atomic<int> dummy_atomic{0};
long long shared_counter = 0;

long long run_without_fence() {
    long long both_entered = 0;

    for (long long i = 0; i < kIterations; ++i) {
        flag_a = false;
        flag_b = false;
        bool a_entered = false;
        bool b_entered = false;

        std::thread t1([&]() {
            flag_a = true;
            if (!flag_b) {
                a_entered = true;
                ++shared_counter;
            }
        });

        std::thread t2([&]() {
            flag_b = true;
            if (!flag_a) {
                b_entered = true;
                ++shared_counter;
            }
        });

        t1.join();
        t2.join();

        if (a_entered && b_entered) {
            ++both_entered;
        }
    }

    return both_entered;
}

long long run_with_fence() {
    long long both_entered = 0;

    for (long long i = 0; i < kIterations; ++i) {
        flag_a = false;
        flag_b = false;
        bool a_entered = false;
        bool b_entered = false;

        std::thread t1([&]() {
            flag_a = true;
            // dummy_atomic gives the fence something to attach to.
            // A standalone fence with no atomic anywhere has no effect.
            dummy_atomic.fetch_add(0, std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_seq_cst);
            if (!flag_b) {
                a_entered = true;
                ++shared_counter;
            }
        });

        std::thread t2([&]() {
            flag_b = true;
            dummy_atomic.fetch_add(0, std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_seq_cst);
            if (!flag_a) {
                b_entered = true;
                ++shared_counter;
            }
        });

        t1.join();
        t2.join();

        if (a_entered && b_entered) {
            ++both_entered;
        }
    }

    return both_entered;
}

int main(int argc, char** argv) {
    if (argc != 2 || (std::strcmp(argv[1], "no-fence") != 0 && std::strcmp(argv[1], "with-fence") != 0)) {
        std::cout << "Usage: " << argv[0] << " [no-fence|with-fence]\n";
        return 1;
    }

    /*
       Each mode runs in its own process invocation, never combined in
       one run, the same separation used for every other good versus bad
       comparison in this repo.
    */
    if (std::strcmp(argv[1], "no-fence") == 0) {
        shared_counter = 0;
        long long both_entered = run_without_fence();
        std::cout << "no-fence: both threads entered together " << both_entered
                   << " times out of " << kIterations << " iterations\n";
        std::cout << "no-fence: shared_counter ended at " << shared_counter
                   << " (expected " << kIterations << " if mutual exclusion held)\n";
    } else {
        shared_counter = 0;
        long long both_entered = run_with_fence();
        std::cout << "with-fence: both threads entered together " << both_entered
                   << " times out of " << kIterations << " iterations\n";
        std::cout << "with-fence: shared_counter ended at " << shared_counter
                   << " (expected " << kIterations << " if mutual exclusion held)\n";
    }

    return 0;
}
