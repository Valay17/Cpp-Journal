#include <stdexcept>

/*
This file is meant to fail to compile. Same call as udl_bug.cpp, a
runtime-only value passed to the literal operator, but this operator is
consteval instead of constexpr. The exact program that quietly compiled
and shipped a runtime crash in udl_bug.cpp is rejected outright here,
before it ever becomes a binary.
*/
consteval int operator""_pct(unsigned long long n) {
    if (n > 100) throw std::out_of_range("percentage out of range");
    return static_cast<int>(n);
}

int main(int argc, char**) {
    int bad = operator""_pct(150ULL + argc); // argc: not known at compile time
    return bad;
}
