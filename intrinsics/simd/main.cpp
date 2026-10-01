#include <xvec/simd>
#include <iostream>

/*
xvec (github.com/intel/xvec) is Intel's header-only implementation of the
C++26 data-parallel library, std::simd. Its xvec::simd::vec<T,N> and
xvec::simd::mask<T,N> correspond directly to std::simd::vec<T,N> and
std::simd::mask<T,N>, so code written against xvec adapts to a conforming
standard library implementation without changing the programming model.
This file reproduces the post's multiply example and prints both operands
so the result can be checked by eye rather than taken on faith.
*/

using xvec::simd::vec;

int main() {
    const vec<float, 8> values(
        [](auto index) { return static_cast<float>(index + 1); });

    const auto result = values * vec<float, 8>(2.0f);

    std::cout << "values: ";
    for (int i = 0; i < 8; i++) std::cout << values[i] << " ";
    std::cout << "\nresult: ";
    for (int i = 0; i < 8; i++) std::cout << result[i] << " ";
    std::cout << "\n";
}
