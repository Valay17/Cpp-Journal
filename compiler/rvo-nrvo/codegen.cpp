#include <cstdio>
#include <utility>

/*
   Not meant to be run. Compiled to assembly and inspected with objdump,
   to see what NRVO looks like at the instruction level versus what
   happens when it is defeated by std::move.

   make_rvo returns a named local directly. NRVO applies: the object is
   constructed in place at the call site, and the generated code for
   make_rvo contains only the construct call, one puts, nothing else.

   make_no_rvo wraps the return in std::move. NRVO requires a plain
   named local variable, std::move produces an rvalue reference, which
   is a different expression category entirely. The compiler cannot apply
   NRVO, falls back to move construction, and the generated code for
   make_no_rvo contains two puts calls: one for construct, one for move.
   That second call is the overhead that should not exist.
*/

struct MyObject {
    MyObject()                    { puts("construct"); }
    MyObject(const MyObject&)     { puts("copy"); }
    MyObject(MyObject&&) noexcept { puts("move"); }
};

MyObject make_rvo() {
    MyObject a;
    return a;
}

MyObject make_no_rvo() {
    MyObject a;
    return std::move(a);
}
