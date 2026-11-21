#include <cstdint>
#include <cstdio>
#include <type_traits>

/*
Every line here has defined behavior. Arithmetic never happens on a type
smaller than int: the operands are promoted to int first, so the result of
a + b is an int even when both sides are uint8_t. The static_asserts check
that at compile time for the five types the post names. They only hold
where int is 32 bits and can hold every value of the smaller type, which is
true on x86-64 Linux.

The rest prints what each conversion produces: the sum before and after it
is stored back into a smaller type, a signed value compared against an
unsigned one, and a double stored into an int.
*/

template <class T>
using Sum = decltype(T{} + T{});

static_assert(std::is_same_v<Sum<std::uint8_t>, int>);
static_assert(std::is_same_v<Sum<std::uint16_t>, int>);
static_assert(std::is_same_v<Sum<char>, int>);
static_assert(std::is_same_v<Sum<short>, int>);
static_assert(std::is_same_v<Sum<bool>, int>);

int main() {
    std::uint8_t a = 200, b = 100;
    auto sum = a + b;                       // both promoted to int: 300
    std::uint8_t back = a + b;              // demoted on store: 300 mod 256 = 44
    std::int8_t back_signed = a + b;        // same cut, into a signed 8-bit type
    printf("a + b = %d, type is int: %d\n", sum, std::is_same_v<decltype(sum), int>);
    printf("uint8_t back = a + b -> %d\n", back);
    printf("int8_t back_signed = a + b -> %d\n", back_signed);

    int neg = -1;
    bool less = neg < 1u;                   // -1 becomes 4294967295: false
    printf("-1 becomes %u, so -1 < 1u is %s\n", static_cast<unsigned>(neg), less ? "true" : "false");

    double d = 3.99;
    int t = d;                              // fraction dropped: 3
    printf("int t = 3.99 -> %d\n", t);
}
