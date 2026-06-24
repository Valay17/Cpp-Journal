#include <atomic>

/*
   Two functions, compiled to assembly and compared with objdump.

   no_fence does a relaxed atomic store with nothing else around it.
   with_seq_cst_fence does the same store, plus a standalone seq_cst
   fence right after.

   On x86, expect no_fence to compile down to a single plain store
   instruction, no special fence instruction involved, since a relaxed
   atomic store needs no ordering guarantees beyond atomicity itself.

   with_seq_cst_fence should show extra instructions around the store,
   typically something equivalent to a locked instruction or an explicit
   MFENCE, because seq_cst is the one ordering that actually needs the
   CPU to do something, not just tell the compiler not to reorder.

   This file is not meant to be run. It exists to be compiled to
   assembly and inspected. See the README for the exact objdump command
   and what to look for in the output.
*/

std::atomic<int> counter{0};

void no_fence() {
    counter.store(1, std::memory_order_relaxed);
}

void with_seq_cst_fence() {
    counter.store(1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_seq_cst);
}
