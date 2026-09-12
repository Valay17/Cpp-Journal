#include <new>
#include <iostream>

/*
The one confirmed break in this entire repo entry. Requires Clang
specifically, GCC has no equivalent flag and none of the eleven other
attempts documented in the README triggered anything with it.

p is constructed as a DerivedA, destroyed, and a DerivedB is
placement-newed into the exact same storage. p itself is never
reassigned, it still points at the same address, but the object actually
living there now is a DerivedB, not a DerivedA. Calling p->id() without
laundering should read 2, DerivedB's answer, since that's the object
actually living at that address now.

Compiled with Clang's -fstrict-vtable-pointers, an optimization built
specifically around this class of UB (documented in WG21 paper P3006R1),
the call gets devirtualized based on the stale provenance from the
original construction instead of the object that's actually there now.
It prints 1, DerivedA's answer, the wrong one.

Build and run:
  clang++ -O2 -fstrict-vtable-pointers -std=c++26 vtable_strict.cpp -o vtable_strict
  ./vtable_strict

Expected output: 1 (wrong, should be 2)
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

    std::cout << p->id() << "\n"; // no launder
}
