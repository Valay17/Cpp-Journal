# Integer Promotion and Implicit Conversions

Full writeup: https://valay17.github.io/Portfolio/blog/language/implicit-conversions

Add two `uint8_t` values, 200 and 100, and the result is 300. A `uint8_t` tops out at 255, so where does the 300 fit?

In C++, arithmetic does not happen on types smaller than `int`. Before the `+` runs, both values are promoted to `int`. That covers `uint8_t`, `uint16_t`, `char`, `short` and `bool`. So `a + b` is an `int`, and 300 fits without trouble.

When the two sides have different types, the compiler converts both to a common one. Put -1 next to an unsigned 1 and the -1 becomes 4,294,967,295, so `-1 < 1u` is false.

Going the other way is demotion, and the value gets cut down. Store that 300 back into a `uint8_t` or an `int8_t` and you get 44, which is 300 mod 256. Float to int drops the fraction, so 3.99 becomes 3. If the float is too big for an int, the behavior is undefined.

The easiest trap to miss is widening too late. With `n` set to 100000, `long long x = n * n` multiplies in `int` first. That overflows, which is undefined behavior, and only then widens. The fix is to widen one operand before the operation, as in `(long long)n * n`.

## What the code demonstrates

Four files, one per group of claims.

`promotion.cpp` has defined behavior on every line. `static_assert`s check at compile time that the sum of two values of each of the five small types is an `int`, and the program prints the sum, the sum stored back into a `uint8_t` and an `int8_t`, the signed against unsigned comparison, and the double stored into an int.

`widening.cpp` runs `n * n` and `(long long)n * n` side by side. `n` comes from the command line so the compiler can't fold the product at compile time. It is run once plainly and once with the undefined behavior sanitizer.

`floatcast.cpp` converts a double that is too big for an `int`, and is run plainly, with `-fsanitize=undefined` and with `-fsanitize=float-cast-overflow`.

`warnings.cpp` is not run. It holds one function per case and is compiled with the conversion warnings on, to see which cases the compiler reports.

## Key insight

```cpp
uint8_t a = 200, b = 100;
auto sum = a + b;                       // both promoted to int: 300
uint8_t back = a + b;                   // demoted on store: 300 mod 256 = 44

int neg = -1;
bool less = neg < 1u;                   // -1 becomes 4294967295: false

double d = 3.99;
int t = d;                              // fraction dropped: 3

int n = 100000;
long long wrong = n * n;                // multiplied as int (overflow), then widened
long long right = (long long)n * n;     // widened first: 10000000000
```

## Run: promotion.cpp

```bash
g++ -std=c++20 -O2 promotion.cpp -o promotion && ./promotion
```
`-O2` is the standard optimization level used across this repo.

```
a + b = 300, type is int: 1
uint8_t back = a + b -> 44
int8_t back_signed = a + b -> 44
-1 becomes 4294967295, so -1 < 1u is false
int t = 3.99 -> 3
```
The sum is 300 and its type is `int`. The same 300 stored into an 8-bit type becomes 44, whether the type is unsigned or signed, since 300 mod 256 is 44. The -1 is converted to 4294967295 before the comparison, so it is not less than 1. The 3.99 loses its fraction.

## Run: widening.cpp

```bash
g++ -std=c++20 -O2 widening.cpp -o widening && ./widening
g++ -std=c++20 -O2 -fsanitize=undefined widening.cpp -o widening_san && ./widening_san
```
`-fsanitize=undefined` adds run-time checks for undefined behavior and prints a message when one is hit.

```
wrong = 1410065408
right = 10000000000
widening.cpp:18:25: runtime error: signed integer overflow: 100000 * 100000 cannot be represented in type 'int'
wrong = 1410065408
right = 10000000000
```
The first two lines are the plain run, the next three the sanitizer run. `wrong` holds 1410065408 because 10,000,000,000 minus two times 4,294,967,296 is 1,410,065,408, the product cut down to 32 bits and then widened. That value is what this build produced, not something the language guarantees, since the overflow is undefined behavior. The sanitizer reports the overflow on the multiplication in `wrong`, and nothing is reported for `right`, where the multiplication is done in `long long`.

## Run: floatcast.cpp

```bash
g++ -std=c++20 -O2 floatcast.cpp -o floatcast && ./floatcast
g++ -std=c++20 -O2 -fsanitize=undefined floatcast.cpp -o floatcast_ubsan && ./floatcast_ubsan
g++ -std=c++20 -O2 -fsanitize=float-cast-overflow floatcast.cpp -o floatcast_san && ./floatcast_san
```

```
int t = 1e+10 -> -2147483648
int t = 1e+10 -> -2147483648
floatcast.cpp:14:13: runtime error: 1e+10 is outside the range of representable values of type 'int'
int t = 1e+10 -> -2147483648
```
The 1e10 is too big for an `int`, so the conversion is undefined behavior, and this build produced -2147483648, the smallest `int`. That is what happened here, not something to rely on. `-fsanitize=undefined` does not report it, and the second line is identical to the plain run. The check has to be asked for by name with `-fsanitize=float-cast-overflow`, which reports the conversion.

## Run: warnings.cpp

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Wconversion -Wsign-compare -c warnings.cpp -o warnings.o
```
`-c` compiles to an object file without linking, since this file has no `main`. `-Wconversion` warns when a conversion can change a value, and `-Wsign-compare` warns when a signed value is compared against an unsigned one. `-Wall` and `-Wextra` are the usual baseline.

```
warnings.cpp: In function ‘int8_t sum_demoted_signed(uint8_t, uint8_t)’:
warnings.cpp:21:26: warning: conversion from ‘int’ to ‘int8_t’ {aka ‘signed char’} may change value [-Wconversion]
   21 |     std::int8_t back = a + b;
      |                        ~~^~~
warnings.cpp: In function ‘uint8_t int_demoted(int)’:
warnings.cpp:26:25: warning: conversion from ‘int’ to ‘uint8_t’ {aka ‘unsigned char’} may change value [-Wconversion]
   26 |     std::uint8_t back = v;
      |                         ^
warnings.cpp: In function ‘bool compare_signed_unsigned(int, unsigned int)’:
warnings.cpp:31:16: warning: comparison of integer expressions of different signedness: ‘int’ and ‘unsigned int’ [-Wsign-compare]
   31 |     return neg < one;
      |            ~~~~^~~~~
warnings.cpp: In function ‘int truncate(double)’:
warnings.cpp:35:13: warning: conversion from ‘double’ to ‘int’ may change value [-Wfloat-conversion]
   35 |     int t = d;
      |             ^
```
Four of the six functions are reported. The `int8_t` demotion, the `int` stored into a `uint8_t`, the signed against unsigned comparison and the double to int are each flagged. Two are not. Line 16, `std::uint8_t back = a + b` from two `uint8_t` values, is the 300 to 44 case, and it compiles without a warning. Line 40, `long long wrong = n * n`, is the late widening, and it compiles without a warning too. In this build the sanitizer is what catches `n * n`.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 16.1.0
