#include <new>
#include <iostream>

/*
Identical to vtable_strict.cpp in every way except one added
std::launder. Same DerivedA-to-DerivedB reconstruction, same Clang flag,
same everything, the only change is std::launder(p) in place of the
stale p when calling id().

Where vtable_strict.cpp prints 1 (wrong), this prints 2 (correct).
std::launder reestablishes provenance, blocking the exact devirtualization
that broke the unlaundered version, and forcing the call to read the
object that's actually there right now.

Build and run:
  clang++ -O2 -fstrict-vtable-pointers -std=c++26 vtable_strict_fixed.cpp -o vtable_strict_fixed
  ./vtable_strict_fixed

Expected output: 2 (correct)
*/

struct Base {
    virtual int id() const { return 0; }
    virtual ~Base() = default;
};
struct DerivedA : Base { int id() const override { return 1; } };
struct DerivedB : Base { int id() const override { return 2; } };

int main() {
    alignas(DerivedA) unsigned char buf[sizeof(DerivedA) > sizeof(DerivedB) ? sizeof(DerivedA) : sizeof(DerivedB)];

    Base* p = new (buf) DerivedA();
    p->~Base();
    new (buf) DerivedB();

    std::cout << std::launder(p)->id() << "\n"; // laundered
}
