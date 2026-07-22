# std::array: Same Speed, Opt-in Safety

Full writeup: https://valay17.github.io/Portfolio/blog/stl/std-array

A C-style array gives you speed with no guardrails. `std::array` gives you the exact same speed, with the guardrails put back, if you actually use them.

`std::array<T, N>` stores its elements inline, exactly like `T arr[N]` does, no pointer to the heap, no separate size or capacity tracking. Write the same loop over a C array and over a `std::array` of the same size, and the compiler emits the identical instructions for both. None of the class wrapper survives compilation, it's a raw array the entire time, the class only exists for the compiler's type checker, not for the CPU.

Here's where people shoot themselves in the foot anyway. `std::array<int, 10> arr; arr[500] = 1;` compiles cleanly and often runs without any visible error, `operator[]` does no bounds checking at all, same as a C array. `arr.size()` still reports 10 right after, since size is just a stored constant, it has no idea whether that write actually landed somewhere valid. `std::vector` has the exact same gap, `vec[500] = 1` on a 10-element vector is just as silent. The safety `std::array` offers over a raw C array isn't automatic, it's opt-in: `.at()` bounds-checks and throws, `operator[]` doesn't check anything, ever.

`N` is part of the type itself, a compile-time constant, so `std::array<int, 8>` and `std::array<int, 16>` are different types, which is what lets everything about the container be known at compile time with zero runtime cost.

## What the code demonstrates

Two small demonstrations, not a speed benchmark, since there is no speed difference to benchmark.

`codegen.cpp` is not run. It compiles two functions, one summing a C array, one summing a `std::array` of the same size, and disassembles both to confirm the compiler treats them identically.

`main.cpp` runs the safety gap directly: an out-of-bounds write through `operator[]` on both `std::array` and `std::vector`, followed by the same out-of-bounds access through `.at()`, to show the exception firing exactly where `operator[]` stayed silent.

## Key insight

```cpp
std::array<int, 10> arr{};
arr[500] = 1;              // undefined behavior, no check, may not even crash
std::cout << arr.size();   // still prints 10

try {
    arr.at(500) = 1;        // this one actually checks
} catch (const std::out_of_range& e) {
    std::cout << "caught: " << e.what();
}
```

Same container, same out-of-bounds index, two completely different outcomes depending only on which access method is used.

## Run: codegen.cpp

```bash
g++ -O2 -std=c++20 -c codegen.cpp -o codegen.o
objdump -d -M intel --no-show-raw-insn codegen.o | grep -A 15 "sum_c_array"
objdump -d -M intel --no-show-raw-insn codegen.o | grep -A 15 "sum_std_array"
```
`-c` compiles to an object file without linking, since this file has no `main` and is not meant to run. `-M intel` selects Intel syntax. `--no-show-raw-insn` hides the raw instruction bytes. `grep -A 15` isolates each function individually. Expect the instruction sequence to match exactly between the two, aside from the base address.

## Run: main.cpp

```bash
g++ -O2 -std=c++20 main.cpp -o main && ./main
```
No special flags needed. Expect `arr.size()` and `vec.size()` to both still report their original size after the out-of-bounds `operator[]` write, followed by a caught `std::out_of_range` exception from each container's `.at()` call at the same index.

## Output

```
$ ./main
arr.size() after OOB write via operator[]: 10
caught: array::at: __n (which is 500) >= _Nm (which is 10)
vec.size() after OOB write via operator[]: 10
caught: vector::_M_range_check: __n (which is 500) >= this->size() (which is 10)
```

```
0000000000000000 <sum_c_array(int const (&) [10])>:
   0:   movdqu xmm0,XMMWORD PTR [rdi+0x10]
   5:   movdqu xmm2,XMMWORD PTR [rdi]
   9:   paddd  xmm0,xmm2
   d:   movdqa xmm1,xmm0
  11:   psrldq xmm1,0x8
  16:   paddd  xmm0,xmm1
  1a:   movdqa xmm1,xmm0
  1e:   psrldq xmm1,0x4
  23:   paddd  xmm0,xmm1
  27:   movd   eax,xmm0
  2b:   add    eax,DWORD PTR [rdi+0x20]
  2e:   add    eax,DWORD PTR [rdi+0x24]
  31:   ret

0000000000000040 <sum_std_array(std::array<int, 10ul> const&)>:
  40:   movdqu xmm0,XMMWORD PTR [rdi+0x10]
  45:   movdqu xmm2,XMMWORD PTR [rdi]
  49:   paddd  xmm0,xmm2
  4d:   movdqa xmm1,xmm0
  51:   psrldq xmm1,0x8
  56:   paddd  xmm0,xmm1
  5a:   movdqa xmm1,xmm0
  5e:   psrldq xmm1,0x4
  63:   paddd  xmm0,xmm1
  67:   movd   eax,xmm0
  6b:   add    eax,DWORD PTR [rdi+0x20]
  6e:   add    eax,DWORD PTR [rdi+0x24]
  71:   ret
```
This build auto-vectorized the loop into a 4-wide SIMD horizontal sum, `movdqu`/`paddd` over 128-bit registers, rather than the plain scalar loop shown in the source. That is a separate optimization decision, unrelated to the `std::array` question, and it lands exactly the same way in both functions, same instructions, same order, same registers, only the base address differs. Whatever the compiler decides to do with this loop, it makes the identical decision for the C array and the `std::array`, which is a stronger confirmation of the core claim than matching scalar code would have been, the compiler is not just failing to add overhead, it is not distinguishing between the two at all.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 16.1.0
