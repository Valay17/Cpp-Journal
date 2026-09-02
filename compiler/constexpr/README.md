# constexpr: Allowed to Run at Compile Time, Not Required To

Full writeup: https://valay17.github.io/Portfolio/blog/compiler/constexpr

`constexpr` doesn't guarantee your code runs at compile time. It guarantees it's allowed to.

Call a `constexpr` function with an argument the compiler doesn't know ahead of time, and it just runs at runtime like any ordinary function, actual instructions, actual work, nothing computed in advance. Call the exact same function in a context that requires a compile-time value, and the entire computation gets folded into a constant before the program ever runs, the result shows up baked directly into the binary, no computation left behind at all.

Since C++20, `constexpr` functions can go further than plain arithmetic. `std::vector` and `std::string`, heap-allocating containers, are usable at compile time now, with one exact restriction that makes the whole thing work: any memory allocated inside a `constexpr` function has to be fully released before that function returns. Build a `std::vector` inside a `constexpr` function, read from it, compute something with it, and return a plain result, that's fine. Return the vector itself and try to keep it alive as a `constexpr` variable, and the compiler refuses outright, a heap allocation surviving past the point where compile time ends isn't allowed.

## What the code demonstrates

Three files. `codegen.cpp` is not run, it compiles and disassembles the same `square` function called two different ways, to show the same source producing different generated code depending purely on how it's called. `main.cpp` runs a `constexpr` function that allocates and destroys a `std::vector` entirely internally, forced to evaluate at compile time via `static_assert`, confirming heap use inside `constexpr` works under the C++20 rules. `fail.cpp` is meant to fail to compile, that failure is the point, it tries to keep a heap-allocated `std::vector` alive as a `constexpr` variable, and the compiler's own error message is the demonstration.

## Key insight

```cpp
constexpr int square(int n) { return n * n; }

int runtime_call(int x) {
    return square(x);       // x unknown at compile time
}

int compile_time_call() {
    constexpr int result = square(5);   // forced compile-time context
    return result;
}
```

Same function, same source, two different fates, decided entirely by the calling context, not by the `constexpr` keyword on `square` itself.

## Run: codegen.cpp

```bash
g++ -O2 -std=c++20 -c codegen.cpp -o codegen.o
objdump -d -M intel --no-show-raw-insn codegen.o
```
`-c` compiles to an object file without linking, since this file has no `main` and is not meant to run. Expect `runtime_call` to contain an actual `imul` instruction, and `compile_time_call` to contain only a `mov` loading the already-computed constant, no multiply anywhere in that function.

## Run: main.cpp

```bash
g++ -O2 -std=c++20 main.cpp -o main && ./main
```
The `static_assert` in this file only compiles if `sum_via_vector(5)` evaluates to `10` at compile time, if it didn't run at compile time at all, this file fails to build rather than silently falling back to a runtime check. Expect the build to succeed and the program to print `sum_via_vector(5) = 10`.

## Run: fail.cpp

```bash
g++ -O2 -std=c++20 -c fail.cpp -o fail.o
```
This is expected to fail. The exact wording of the error differs by compiler, but the point stands: it should name an `operator new` result or heap allocation as the reason the expression is not a valid constant expression.

## Output

```
$ ./main
sum_via_vector(5) = 10
```

```
$ objdump -d -M intel --no-show-raw-insn codegen.o

0000000000000000 <runtime_call(int)>:
   0:   imul   edi,edi
   3:   mov    eax,edi
   5:   ret

0000000000000010 <compile_time_call()>:
  10:   mov    eax,0x19
  15:   ret
```
`runtime_call` contains an actual `imul`, the multiply happens when the program runs. `compile_time_call` is two instructions total, `mov eax,0x19` loads 25 directly, `0x19` in hex, no multiply anywhere in the function, the entire computation was already done before this code ever executed.

```
$ g++ -O2 -std=c++20 -c fail.cpp -o fail.o
fail.cpp: In function 'int main()':
fail.cpp:20:54: error: 'make_vector(5)' is not a constant expression because it refers to a result of 'operator new'
   20 |     constexpr std::vector<int> result = make_vector(5);
      |                                                      ^
In file included from .../include/c++/16.1.0/vector:65,
                 from fail.cpp:1:
.../include/c++/16.1.0/bits/allocator.h:203:52: note: allocated here
  203 |             return static_cast<_Tp*>(::operator new(__n));
      |                                      ~~~~~~~~~~~~~~^~~~~
```
The compiler names the exact allocation that's still alive at the point `make_vector` returns, and refuses to treat the result as a constant expression because of it. This is the C++20 rule enforced directly, not a vague restriction, the error points at the specific `operator new` call responsible.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 16.1.0
