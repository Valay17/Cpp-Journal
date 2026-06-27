#include <chrono>
#include <cstring>
#include <iostream>
#include <pthread.h>
#include <thread>
#include <vector>

constexpr long long kPasses = 4000;
constexpr size_t kWorkingSetBytes = 256 * 1024; // fits in L2, exceeds L1
constexpr size_t kWorkingSetInts = kWorkingSetBytes / sizeof(int);
constexpr long long kMigrationIntervalPasses = 50;

/*
   Forces the exact scenario the post describes, rather than waiting for
   the OS scheduler to maybe do it. pinned mode sets affinity to one core
   for the entire run and never touches it again. migrating mode starts
   pinned to one core, then explicitly flips affinity to a different
   core at a fixed interval throughout the run, repeatedly, simulating a
   thread the scheduler keeps bouncing around.

   The work itself is a repeated sweep over a fixed-size array, sized to
   fit comfortably in L2 but exceed L1 on this CPU. This means the array
   genuinely benefits from staying resident in a core's cache across
   passes. Right after a forced migration, the new core's cache has
   never seen this array, so the first several passes after migrating
   have to rebuild that residency from a colder cache state or from RAM,
   which is the actual cost being measured. A working set that already
   fit in L1 everywhere, or one far larger than any cache level, would
   not show this difference clearly, the size here is chosen specifically
   to make cache residency matter.

   Set affinity to two specific cores below before running. Defaults to
   core 0 and core 1, adjust if this machine's core layout makes a
   different pair more meaningful (e.g. cores on the same physical
   package versus different ones, NUMA effects on multi-socket systems,
   not relevant on this single-socket laptop CPU but worth knowing if
   run elsewhere).
*/

bool set_affinity(pthread_t handle, int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    return pthread_setaffinity_np(handle, sizeof(cpu_set_t), &cpuset) == 0;
}

long long touch_working_set(std::vector<int>& data) {
    long long sum = 0;
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = data[i] * 3 + 1;
        sum += data[i];
    }
    return sum;
}

int main(int argc, char** argv) {
    if (argc != 2 || (std::strcmp(argv[1], "pinned") != 0 && std::strcmp(argv[1], "migrating") != 0)) {
        std::cout << "Usage: " << argv[0] << " [pinned|migrating]\n";
        return 1;
    }

    /*
       core_a and core_b must be separate physical cores, not
       two SMT threads of the same core. On this CPU, lscpu -e showed
       CPU 0 and CPU 1 sharing CORE 0 (same physical core, two SMT
       threads, sharing L1/L2/L3 entirely), which would defeat this
       demo entirely if used as the migration pair. CPU 0 (CORE 0) and
       CPU 4 (CORE 2) are separate physical cores with their
       own L1 and L2, confirmed via lscpu -e on this machine specifically.
       Re-check lscpu -e and adjust these two constants if running on
       different hardware, the right pair is not guaranteed to be 0 and 4
       on every CPU.
    */
    constexpr int core_a = 0;
    constexpr int core_b = 4;

    std::vector<int> data(kWorkingSetInts, 1);
    long long checksum = 0;

    std::thread worker([&]() {
        bool currently_on_a = true;

        for (long long pass = 0; pass < kPasses; ++pass) {
            checksum += touch_working_set(data);

            /*
               Each mode runs in its own process invocation, never
               combined in one run, the same separation used for every
               other good versus bad comparison in this repo.
            */
            if (std::strcmp(argv[1], "migrating") == 0 &&
                pass > 0 && pass % kMigrationIntervalPasses == 0) {
                currently_on_a = !currently_on_a;
                set_affinity(pthread_self(), currently_on_a ? core_a : core_b);
            }
        }
    });

    auto start = std::chrono::steady_clock::now();
    if (!set_affinity(worker.native_handle(), core_a)) {
        std::cerr << "Failed to pin worker thread\n";
    }
    worker.join();
    auto end = std::chrono::steady_clock::now();

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << argv[1] << ": " << ms << " ms, checksum = " << checksum << "\n";

    return 0;
}
