# C++26 std::simd: Multiplying by 2 Isn't a Multiply

Full writeup: https://valay17.github.io/Portfolio/blog/intrinsics/simd

`std::simd` is the data-parallel vector library landing in C++26. Intel's `xvec` (github.com/intel/xvec) is a header-only reference implementation of it: `xvec::simd::vec<T,N>` and `xvec::simd::mask<T,N>` map directly onto `std::simd::vec<T,N>` and `std::simd::mask<T,N>`, so code written against it carries over to a conforming standard library once one ships.

`vec<float,8> values(...)` holds eight floats in one object. `values * vec<float,8>(2.0f)` reads as a vector multiply, eight lanes multiplied by the same scalar. Disassembling it shows `vaddps`, an add instruction, not `vmulps`. That looks like the library picked the wrong opcode, or the compiler failed to use the intended one. Neither is true: `x * 2.0f` and `x + x` compute the same value, and the compiler lowers the multiply into a self-add because it's the same cost and skips materializing the constant `2.0f` into a register. It's a scalar-algebra identity applied to every lane at once, not a `std::simd`-specific trick, and not a bug in Intel's implementation.

## What the code demonstrates

`main.cpp` runs the post's exact example and prints both operands, confirming the multiply computes the correct values regardless of which instruction it compiles to.

`codegen.cpp` is not run. It is compiled to an object file and disassembled, isolating the multiply-by-2 case against two controls: `scale_by_three`, the same operation with a constant that isn't 2, and `add_vecs`, a plain vector add. If `scale_by_two` and `scale_by_three` disassembled to the same instruction, that would mean `vec<float,8>` multiplication always lowers to an add regardless of the constant, which would be the library doing something wrong. They don't: only multiplying by 2 does this. `scale_by_two` and `add_vecs` disassembling to the same instruction confirms what that substitution actually is, the multiply becomes the add it's equivalent to.

## Key insight

```cpp
vec<float, 8> scale_by_two(vec<float, 8> values) {
    return values * vec<float, 8>(2.0f);   // compiles to vaddps, not vmulps
}

vec<float, 8> scale_by_three(vec<float, 8> values) {
    return values * vec<float, 8>(3.0f);   // compiles to vmulps, as expected
}
```

Disassembling one call site only tells you about that operation at that specific constant. A multiply at a different constant, or a multiply where both sides are runtime values, settles whether the first result generalizes before concluding anything about how the operation works.

## Run: main.cpp

```bash
g++ -std=c++20 -O2 -march=x86-64-v3 -I/path/to/xvec/include main.cpp -o main
./main
```
`-march=x86-64-v3` is the baseline the post's example targets; it implies AVX2, which is what gives an 8-wide `float` vector a single 256-bit register (`ymm`) instead of being split across two 128-bit halves. `-I` points at xvec's `include` directory so `#include <xvec/simd>` resolves.

```
values: 1 2 3 4 5 6 7 8 
result: 2 4 6 8 10 12 14 16 
```
Each lane doubled, confirming the multiply is correct regardless of which instruction it compiled to.

## Run: codegen.cpp

```bash
g++ -std=c++20 -O2 -march=x86-64-v3 -I/path/to/xvec/include -c codegen.cpp -o codegen.o
objdump -d -M intel --no-show-raw-insn codegen.o
```
`-c` compiles to an object file without linking, since this file has no `main` and is not meant to run.

```
0000000000000000 <scale_by_two(xvec::simd::basic_vec<...>)>:
   0:   vaddps ymm0,ymm0,ymm0
   4:   ret

0000000000000010 <scale_by_three(xvec::simd::basic_vec<...>)>:
  10:   vbroadcastss ymm1,DWORD PTR [rip+0x0]
  19:   vmulps ymm0,ymm0,ymm1
  1d:   ret

0000000000000020 <add_vecs(xvec::simd::basic_vec<...>, xvec::simd::basic_vec<...>)>:
  20:   vaddps ymm0,ymm0,ymm1
  24:   ret
```
(Symbol names shortened from their mangled form for readability.) `scale_by_two` is one instruction, `vaddps ymm0,ymm0,ymm0`, operand added to itself. `scale_by_three` needs a second register loaded with the constant 3.0f (`vbroadcastss`) before `vmulps` runs on it, confirming multiplication by an arbitrary constant does go through an actual multiply. `add_vecs`'s body is `vaddps ymm0,ymm0,ymm1`, the same instruction `scale_by_two` uses, with its second operand register swapped for the multiply-by-2 case's self-reference.

## Clang comparison

```bash
clang++ -std=c++20 -O2 -march=x86-64-v3 -I/path/to/xvec/include main.cpp -o main_clang && ./main_clang
clang++ -std=c++20 -O2 -march=x86-64-v3 -I/path/to/xvec/include -c codegen.cpp -o codegen_clang.o
objdump -d -M intel --no-show-raw-insn codegen_clang.o
```
Same values, same three instructions (`vaddps` for `scale_by_two`, `vmulps` for `scale_by_three`, `vaddps` for `add_vecs`), confirming this is an ordinary strength-reduction optimization both compilers apply, not a quirk of one toolchain or of xvec's implementation. Clang additionally emits `push rbp` / `mov rbp,rsp` / `pop rbp` around each function; GCC does not. That is a frame pointer convention difference between the two compilers on these functions, unrelated to the multiply-to-add substitution.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 16.1.0
- Compiler: clang version 21.1.8
- Library: xvec (github.com/intel/xvec), header-only reference implementation of C++26 `std::simd`
