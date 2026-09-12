#include <iostream>
#include <new>

/*
Every pattern in this file was tested at -O0 through -O3, on two GCC
versions (13.3.0 and 16.1.0), specifically trying to make omitting
std::launder produce a visibly wrong result. None of them did. That is
the actual finding this entry documents, not a failed attempt to hide.
See the README for the full list of what was tried, including a
cross-translation-unit LTO build that isn't reproducible in a single
file and lives in lib.cpp / lto_main.cpp instead.
*/

// destroying a float and placement-newing an int into the same storage
void float_to_int_example() {
    alignas(int) unsigned char buf[sizeof(int)];
    using FloatT = float;

    float* pf = new (buf) float(3.14f);
    pf->~FloatT();

    int* pi = new (buf) int(69);
    (void)pi;

    unsigned char* raw = buf;
    int* stale = reinterpret_cast<int*>(raw);
    int* fixed = std::launder(stale);

    std::cout << "float_to_int_example: via reinterpret_cast (UB) = " << *stale
              << ", via std::launder = " << *fixed << "\n";
}

// the textbook motivating case from the C++17 proposal that introduced
// std::launder: a const member, reconstructed with a different value in
// the same storage
struct X { const int n; };

void const_member_example() {
    alignas(X) unsigned char buf[sizeof(X)];
    X* p = new (buf) X{1};

    int a = p->n;

    p->~X();
    new (buf) X{2};

    int b = p->n;
    int c = std::launder(p)->n;

    std::cout << "const_member_example: a=" << a << " b=" << b << " c=" << c << "\n";
}

// reconstructed repeatedly in a loop, tried specifically to trigger
// loop-invariant code motion hoisting a stale read out of the loop body
void loop_example() {
    alignas(X) unsigned char buf[sizeof(X)];
    X* p = new (buf) X{0};

    std::cout << "loop_example (no launder): ";
    for (int i = 0; i < 5; i++) {
        p->~X();
        new (buf) X{i};
        std::cout << p->n << " ";
    }
    std::cout << "\n";
}

// polymorphic reconstruction, tried specifically to trigger
// devirtualization based on a stale static type assumption
struct Base {
    virtual int id() const { return 0; }
    virtual ~Base() = default;
};
struct DerivedA : Base { int id() const override { return 1; } };
struct DerivedB : Base { int id() const override { return 2; } };

void virtual_dispatch_example() {
    alignas(DerivedA) unsigned char buf[sizeof(DerivedA) > sizeof(DerivedB) ? sizeof(DerivedA) : sizeof(DerivedB)];

    Base* p = new (buf) DerivedA();
    int first = p->id();

    p->~Base();
    Base* p2 = new (buf) DerivedB();

    int no_launder = p->id();
    int with_launder = std::launder(p)->id();
    int direct = p2->id();

    std::cout << "virtual_dispatch_example: first=" << first
              << " no_launder=" << no_launder
              << " with_launder=" << with_launder
              << " direct=" << direct << "\n";
}

int main() {
    float_to_int_example();
    const_member_example();
    loop_example();
    virtual_dispatch_example();
}
