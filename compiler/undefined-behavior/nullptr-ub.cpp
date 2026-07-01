#include <iostream>

/*
   Dereferencing a null pointer is undefined behavior. The compiler is
   allowed to assume it never happens in a correct program, the same
   freedom it takes with signed integer overflow. This means a null
   check that appears AFTER a dereference of the same pointer can be
   silently removed: the dereference already "told" the compiler the
   pointer was non-null at that point, so the check below it is
   provably false and gets eliminated.

   read_value dereferences ptr first, then checks if it was null.
   Under the UB-means-impossible rule, the compiler sees the
   dereference and concludes ptr cannot be null, which makes the
   check immediately below it dead code.

   This file is compiled three separate ways:

   1. Default build (-O0 and -O2): expect the null check to survive
      at -O0 (minimal reasoning) and disappear at -O2 (the compiler
      applies the UB-based proof and removes it).

   2. With -fno-delete-null-pointer-checks: tells the compiler not to
      use pointer dereferences as proof of non-nullness. The null check
      should reappear at both optimization levels.

   3. With -fsanitize=undefined: instruments the binary to catch the
      null dereference at runtime and report it directly.
*/

int read_value(int* ptr) {
    int val = *ptr;        // UB if ptr is null, compiler assumes it isn't
    if (ptr == nullptr) {  // compiler may eliminate this entirely
        return -1;
    }
    return val;
}

int main() {
    std::cout << read_value(nullptr) << "\n";
}
