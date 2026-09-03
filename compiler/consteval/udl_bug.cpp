#include <iostream>
#include <stdexcept>

/*
This function is meant to validate at compile time: throwing inside a
constexpr function during constant evaluation makes the expression
ill-formed, which is a well known trick for rejecting bad literals at
compile time. The catch is that constexpr only permits compile-time
evaluation, it does not require it. Called with a value the compiler is
not forced to resolve ahead of time, the throw becomes an ordinary
runtime throw instead of a compile error, and the program that called it
with an invalid value still compiles successfully.
*/
constexpr int operator""_pct(unsigned long long n) {
    if (n > 100) throw std::out_of_range("percentage out of range");
    return static_cast<int>(n);
}

int main(int argc, char**) {
    // called as a plain function with a runtime value, not through literal
    // syntax, argc is not known at compile time, and 150 + argc is always
    // out of range for any normal argc
    int bad = operator""_pct(150ULL + argc);
    std::cout << "bad = " << bad << "\n";
}
