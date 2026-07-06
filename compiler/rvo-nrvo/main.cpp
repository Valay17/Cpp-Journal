#include <cstdio>
#include <utility>

/*
   Demonstrates RVO and NRVO through constructor call output.

   MyObject prints which constructor fires so the difference between
   make_rvo and make_no_rvo is visible directly at runtime without
   needing to read assembly. make_rvo returns a named local variable
   directly, NRVO applies, and only the constructor fires. make_no_rvo
   wraps the return in std::move, which hands the compiler a complex
   expression instead of a plain named local. NRVO requires a plain
   named local, std::move produces neither, so NRVO is defeated and a
   move constructor call appears that should never have existed.

   The fix is always to return the local variable directly. The compiler
   knows better than the programmer here, and adding std::move is not
   a hint, it is an obstacle.
*/

struct MyObject {
    MyObject()                   { printf("construct\n"); }
    MyObject(const MyObject&)    { printf("copy\n"); }
    MyObject(MyObject&&) noexcept { printf("move\n"); }
};

MyObject make_rvo() {
    MyObject a;
    return a;           // NRVO applies: construct in place, no move
}

MyObject make_no_rvo() {
    MyObject a;
    return std::move(a); // NRVO defeated: move fires unnecessarily
}

int main() {
    printf("=== make_rvo ===\n");
    MyObject x = make_rvo();

    printf("=== make_no_rvo ===\n");
    MyObject y = make_no_rvo();
}
