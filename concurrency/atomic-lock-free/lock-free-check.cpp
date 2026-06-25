#include <atomic>
#include <iostream>

/*
   std::atomic<T> only guarantees atomicity. Whether the implementation
   is lock-free is left up to the platform and the size of the type. The
   only types with a hard, portable guarantee of lock-free behavior are
   atomic_flag, atomic_signed_lock_free, and atomic_unsigned_lock_free.
   For everything else, is_always_lock_free has to be checked, not
   assumed.

   is_always_lock_free is a static constexpr member, not an instance
   member. It belongs to the type, not the object, so it is accessed as
   Type::is_always_lock_free, never object.is_always_lock_free.
*/

struct Small { int x; };       // 4 bytes, fits in one instruction
struct Big   { int data[8]; }; // 32 bytes, no single instruction covers it

int main() {
    std::cout << "atomic<Small>::is_always_lock_free = "
              << std::boolalpha << std::atomic<Small>::is_always_lock_free << "\n";
    std::cout << "atomic<Big>::is_always_lock_free = "
              << std::atomic<Big>::is_always_lock_free << "\n";

    /*
       The three types with a hard, portable guarantee. These do not
       need checking, the standard requires them to be lock-free on any
       conforming implementation.
    */
    std::cout << "atomic_flag is always lock-free by the standard, no "
                 "check needed\n";
    std::cout << "atomic_signed_lock_free::is_always_lock_free = "
              << std::atomic_signed_lock_free::is_always_lock_free << "\n";
    std::cout << "atomic_unsigned_lock_free::is_always_lock_free = "
              << std::atomic_unsigned_lock_free::is_always_lock_free << "\n";

    return 0;
}
