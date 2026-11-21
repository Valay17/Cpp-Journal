#include <cstdint>

/*
This file is not run. It is compiled with the conversion warnings turned on
to see which of the post's cases each warning reports. The functions take
their values as arguments so the compiler cannot see the numbers and has to
judge each line by its types alone.

sum_demoted stores the sum of two uint8_t back into a uint8_t, the 300 to 44
case. sum_demoted_signed stores it into an int8_t. int_demoted stores a plain
int into a uint8_t. compare_signed_unsigned is the -1 < 1u case. truncate is
the double to int case. widen_late is the n * n case.
*/

std::uint8_t sum_demoted(std::uint8_t a, std::uint8_t b) {
    std::uint8_t back = a + b;
    return back;
}

std::int8_t sum_demoted_signed(std::uint8_t a, std::uint8_t b) {
    std::int8_t back = a + b;
    return back;
}

std::uint8_t int_demoted(int v) {
    std::uint8_t back = v;
    return back;
}

bool compare_signed_unsigned(int neg, unsigned one) {
    return neg < one;
}

int truncate(double d) {
    int t = d;
    return t;
}

long long widen_late(int n) {
    long long wrong = n * n;
    return wrong;
}
