#include <array>
#include <cstddef>

/*
Not run, compiled and disassembled only. Both functions do the same thing,
sum ten ints, one over a plain C array reference, one over a std::array
reference of the same size. The point is whether the compiler treats these
as the same thing once compiled, not whether the sum itself is
interesting.
*/

int sum_c_array(const int (&arr)[10]) {
    int total = 0;
    for (size_t i = 0; i < 10; i++)
        total += arr[i];
    return total;
}

int sum_std_array(const std::array<int, 10>& arr) {
    int total = 0;
    for (size_t i = 0; i < 10; i++)
        total += arr[i];
    return total;
}
