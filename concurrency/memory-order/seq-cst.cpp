#include <atomic>
#include <iostream>
#include <thread>

/*
   Same three thread layout as acquire-release.cpp: one writer storing to
   two atomics, two readers reading both atomics in opposite orders.

   The only change is memory_order_seq_cst on every operation instead of
   release/acquire. seq_cst does not just create a wall between the
   writer and each reader individually. It places every seq_cst operation
   in the ENTIRE program into one single global order that all threads
   agree on, including operations on x and operations on y, even though
   they are different atomics.

   That global order is the guarantee acquire/release does not give you.
   With seq_cst, reader1 and reader2 cannot end up implying two different
   orders for when x and y became visible. Both readers are constrained
   by the same single timeline, not two independent walls.
*/

struct State {
    std::atomic<int> x{0};
    std::atomic<int> y{0};
};

int main() {
    State state;

    std::thread writer([&]() {
        state.x.store(1, std::memory_order_seq_cst);
        state.y.store(1, std::memory_order_seq_cst);
    });

    std::thread reader1([&]() {
        int saw_x = state.x.load(std::memory_order_seq_cst);
        std::cout << "reader1: saw x = " << saw_x << "\n";
        int saw_y = state.y.load(std::memory_order_seq_cst);
        std::cout << "reader1: saw y = " << saw_y << "\n";
    });

    std::thread reader2([&]() {
        int saw_y = state.y.load(std::memory_order_seq_cst);
        std::cout << "reader2: saw y = " << saw_y << "\n";
        int saw_x = state.x.load(std::memory_order_seq_cst);
        std::cout << "reader2: saw x = " << saw_x << "\n";
    });

    writer.join();
    reader1.join();
    reader2.join();

    std::cout << "\nWith seq_cst, reader1 and reader2 are placed on the same global "
                 "timeline as the writer's stores to x and y. They cannot disagree "
                 "about the relative order x and y became visible, the way "
                 "acquire/release alone permitted.\n";

    return 0;
}
