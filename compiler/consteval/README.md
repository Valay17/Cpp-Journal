# consteval: A Compile-Time Function That Doesn't Take No for an Answer

Full writeup: https://valay17.github.io/Portfolio/blog/compiler/consteval

There's a version of a compile-time function that doesn't take no for an answer.

`consteval`, introduced in C++20, produces what the standard calls an immediate function. Unlike a plain compile-time-capable function, an immediate function is never allowed to run at runtime, full stop. Call it with something the compiler can't resolve ahead of time, and it isn't slower code falling back to a runtime path, it's a compile error.

This flips the guarantee entirely compared to a function that's merely allowed to run at compile time. That kind can quietly cost you at runtime and you'd never know unless you went looking for it in the generated code, covered in the `constexpr` entry in this repo. A `consteval` function can't hide that from you, if it didn't happen, the binary doesn't exist.

## The user-defined literal bug this actually fixes

`consteval` shows up as the fix for a specific, non-obvious class of bug involving user-defined literals. A literal operator meant to validate its input at compile time, checking it fits within some range, commonly does this by throwing inside the function during constant evaluation, which is a well known trick, throwing inside a constant expression makes the expression ill-formed, rejecting the program at compile time.

The trick only works if the function is actually evaluated at compile time. Marked `constexpr`, it is merely allowed to be, not required to be. Call the operator directly as a plain function, bypassing literal syntax, with a value the compiler is not forced to resolve ahead of time, and the throw becomes an ordinary runtime throw instead of a compile error. A program that passes an invalid value still compiles and ships successfully, the bug only surfaces later, at runtime, potentially in production if that exact path was never exercised during testing.

Marking the same operator `consteval` closes the gap outright. The operator can only ever be called with something the compiler can validate ahead of time, the same call that quietly compiled and shipped a runtime crash under `constexpr` is rejected before it ever becomes a binary under `consteval`.

## Key insight

```cpp
consteval int square(int n) { return n * n; }

int runtime_call(int x) {
    return square(x);   // x isn't known at compile time
}
// error: 'x' is not a constant expression
```

No fallback path, no runtime instructions generated. Either the compiler can prove `x` is a constant expression, or the build fails.

## Run: fail.cpp

```bash
g++ -O2 -std=c++26 -c fail.cpp -o fail.o
```
This is expected to fail. Expect an error naming `x` directly as not being a constant expression.

## Run: udl_bug.cpp

```bash
g++ -O2 -std=c++26 udl_bug.cpp -o udl_bug
./udl_bug
```
This one compiles and runs. `operator""_pct` is called directly as a function with `150ULL + argc`, always out of range for any normal `argc`, but the compiler is never forced to evaluate this call at compile time, so the range check only ever runs when the program executes. Expect the build to succeed and the program to terminate with an uncaught `std::out_of_range`.

## Run: udl_fix_valid.cpp

```bash
g++ -O2 -std=c++26 udl_fix_valid.cpp -o udl_fix_valid
./udl_fix_valid
```
Same validation logic, `consteval` instead of `constexpr`, called through ordinary literal syntax with a value that is in range and known at compile time. Expect this to compile and run normally, confirming the fix does not break legitimate use.

## Run: udl_fix_invalid.cpp

```bash
g++ -O2 -std=c++26 -c udl_fix_invalid.cpp -o udl_fix_invalid.o
```
This is expected to fail. Same call as `udl_bug.cpp`, `150ULL + argc`, but on the `consteval` version. Expect an error naming `argc` directly as not being a constant expression, the same program that compiled and crashed at runtime in `udl_bug.cpp` is rejected here before it exists as a binary.

## Output

```
$ g++ -O2 -std=c++26 -c fail.cpp -o fail.o
fail.cpp: In function 'int runtime_call(int)':
fail.cpp:12:18: error: call to consteval function 'square(x)' is not a constant expression
   12 |     return square(x);
      |            ~~~~~~^~~
fail.cpp:12:19: error: 'x' is not a constant expression
   12 |     return square(x);
      |                   ^
```
Two errors, not one, on this GCC version. The first names the call itself as not being a constant expression, the second names the specific reason, `x`. Same substance as the single-line version, just more detail.

```
$ g++ -O2 -std=c++26 udl_bug.cpp -o udl_bug && ./udl_bug
terminate called after throwing an instance of 'std::out_of_range'
  what():  percentage out of range
Aborted
```
Compiles clean, then crashes the moment it runs. The validation logic exists and fires, it just fires at runtime instead of rejecting the build.

```
$ g++ -O2 -std=c++26 udl_fix_valid.cpp -o udl_fix_valid && ./udl_fix_valid
good = 50
```
`consteval` does not interfere with legitimate, compile-time-known literal use.

```
$ g++ -O2 -std=c++26 -c udl_fix_invalid.cpp -o udl_fix_invalid.o
udl_fix_invalid.cpp: In function 'int main(int, char**)':
udl_fix_invalid.cpp:16:29: error: call to consteval function 'operator""_pct((((long long unsigned int)argc) + 150))' is not a constant expression
   16 |     int bad = operator""_pct(150ULL + argc); // argc: not known at compile time
      |               ~~~~~~~~~~~~~~^~~~~~~~~~~~~~~
udl_fix_invalid.cpp:16:39: error: 'argc' is not a constant expression
   16 |     int bad = operator""_pct(150ULL + argc); // argc: not known at compile time
      |                                       ^
```
The exact same call that quietly compiled and crashed at runtime in `udl_bug.cpp` is rejected here before it becomes a binary at all. `consteval` even echoes the full expression it evaluated, `150 + argc`, before naming `argc` as the specific non-constant piece.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 16.1.0
