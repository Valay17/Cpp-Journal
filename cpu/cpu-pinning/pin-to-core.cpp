#include <pthread.h>
#include <iostream>
#include <thread>

/*
   Pins a thread to a single core. cpu_set_t is a bitmask, one bit per
   core. CPU_ZERO clears it, CPU_SET turns on exactly the bit for
   core_id, and pthread_setaffinity_np hands that mask to the kernel as
   the only set of cores this thread is allowed to run on. Once applied,
   the scheduler cannot move this thread to any other core, regardless
   of what else is happening on the system.

   The same effect is available from the shell with no code at all:
   taskset -c 3 ./my_program
   That pins the entire process, every thread it has, to core 3. The
   function below pins one specific thread, which matters once a
   program has more threads than you want pinned to the same place.
*/

bool pin_to_core(std::thread& t, int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    int result = pthread_setaffinity_np(t.native_handle(), sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
        std::cerr << "pthread_setaffinity_np failed for core " << core_id << "\n";
        return false;
    }
    return true;
}

int main() {
    std::thread worker([]() {
        // placeholder work, this file exists to show pin_to_core in
        // isolation, the actual cost measurement lives in
        // migration-cost.cpp
        volatile long long sum = 0;
        for (long long i = 0; i < 100'000'000; ++i) {
            sum += i;
        }
    });

    if (pin_to_core(worker, 0)) {
        std::cout << "Pinned worker thread to core 0\n";
    }

    worker.join();
    return 0;
}
