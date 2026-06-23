#include <atomic>
#include <iostream>
#include <new>
#include <thread>

constexpr long long kMaxIterations = 2'000'000;

/*
   Three threads. One writer, two readers.

   The writer stores to two different atomics, x and y, using release.
   Reader 1 reads x then y. Reader 2 reads y then x, the opposite order.

   Acquire/release only creates a wall between two threads sharing ONE
   atomic. Reader 1's acquire load of x is paired with the writer's
   release store of x. Reader 1's load of y is a separate, unrelated
   load with no ordering guarantee relative to its own load of x, because
   acquire/release says nothing about ordering BETWEEN two different
   atomics. The wall is per-variable, not global.

   This means reader 1 and reader 2 are allowed to see x and y "arrive"
   in different relative orders. Nothing here is undefined behavior or a
   data race, every operation is a correctly paired atomic access. The
   point is narrower than that: acquire/release alone does not promise
   the two readers will agree on the order the two stores became visible.

   This runs up to 2 million times, stopping the moment reader1 and
   reader2 imply a different order for x and y. If no disagreement shows
   up in 2 million runs, that is reported too. See the README for why
   that is the expected result on x86, not a sign anything is wrong.
*/

struct State {
    alignas(std::hardware_destructive_interference_size) std::atomic<int> x{0};
    alignas(std::hardware_destructive_interference_size) std::atomic<int> y{0};
};

/*
   Encodes which value (x or y) each reader saw arrive first.
   true  = saw x become 1 strictly before y
   false = saw y become 1 strictly before x, or saw them "simultaneously"
           in the sense that both were already 1 by the first read
*/
struct ReaderResult {
    int first_value;
    int second_value;
};

int main() {
    long long run = 0;
    bool found_disagreement = false;

    for (; run < kMaxIterations; run++) {
        State state;
        ReaderResult r1{};
        ReaderResult r2{};

        std::thread writer([&]() {
            state.x.store(1, std::memory_order_release);
            state.y.store(1, std::memory_order_release);
        });

        std::thread reader1([&]() {
            r1.first_value = state.x.load(std::memory_order_acquire);
            r1.second_value = state.y.load(std::memory_order_acquire);
        });

        std::thread reader2([&]() {
            r2.first_value = state.y.load(std::memory_order_acquire);
            r2.second_value = state.x.load(std::memory_order_acquire);
        });

        writer.join();
        reader1.join();
        reader2.join();

        /*
           reader1 read order: x, then y
           reader2 read order: y, then x

           Disagreement case: reader1 saw x arrive but not yet y
           (x=1, y=0), while reader2 saw y arrive but not yet x
           (y=1, x=0). That is two readers each catching the OTHER
           variable still at its old value while their own "first read"
           already shows the new value, which only makes sense if x and
           y became visible in two different relative orders to the two
           readers.
        */
        bool reader1_saw_x_only = (r1.first_value == 1 && r1.second_value == 0);
        bool reader2_saw_y_only = (r2.first_value == 1 && r2.second_value == 0);

        if (reader1_saw_x_only && reader2_saw_y_only) {
            found_disagreement = true;
            std::cout << "Disagreement found on run " << run << "\n";
            std::cout << "reader1: saw x = " << r1.first_value << ", then y = " << r1.second_value << "\n";
            std::cout << "reader2: saw y = " << r2.first_value << ", then x = " << r2.second_value << "\n";
            std::cout << "reader1 implies x became visible before y. reader2 implies y "
                         "became visible before x. Acquire/release does not forbid this, "
                         "since x and y are two separate walls, not one global order.\n";
            break;
        }
    }

    if (!found_disagreement) {
        std::cout << "No disagreement found in " << run << " runs. See the README for why "
                     "this is the expected result on x86, not evidence that the ordering "
                     "guarantee exists.\n";
    }

    return 0;
}
