#include <iostream>
#include <stdexcept>

/*
Same validation logic as the constexpr version, marked consteval instead.
A valid literal known at compile time still works exactly as expected,
consteval does not break ordinary use, it only closes the gap that let a
bad value slip through as a runtime throw instead of a compile error.
*/
consteval int operator""_pct(unsigned long long n) {
    if (n > 100) throw std::out_of_range("percentage out of range");
    return static_cast<int>(n);
}

int main() {
    int good = 50_pct; // literal syntax, in range, known at compile time
    std::cout << "good = " << good << "\n";
}
