# constinit: Locks the Start, Not the Value

Full writeup: https://valay17.github.io/Portfolio/blog/compiler/constinit

A variable can be guaranteed to start life with a compile-time value and still be completely mutable afterward. Those are two separate promises, and `constinit` only makes one of them.

Introduced in C++20, `constinit` applies only to variables with static or thread-local storage, never functions. What it guarantees is narrow: the variable's initial value has to come from a compile-time constant expression, no dynamic initialization, no depending on some function call resolving at runtime.

Try to initialize a `constinit` variable from an ordinary, non-`constexpr` function, and the compiler stops immediately, naming the exact function that isn't allowed there. There's no fallback, the value either exists at compile time or the build fails.

What `constinit` deliberately doesn't guarantee is immutability. A `constinit` variable can be freely modified after that initial value is locked in, plain assignment, no different from any other variable, it just can't start out uninitialized or dependent on runtime setup. That's a different promise than `const` or `constexpr`, both of which lock a variable's value for its entire lifetime.

This makes `constinit` a targeted tool against a specific class of bug, a global variable whose starting value silently depends on some other object's constructor having already run, with no guarantee that ordering holds across separate files. Forcing the starting value to exist at compile time removes that dependency outright, there's nothing left for it to race against.

## What the code demonstrates

Three files. `fail.cpp` is meant to fail to compile, it's the post's own `bad`/`good` example directly, `bad` calls an ordinary function and gets rejected, `good` calls a `constexpr` function and would be fine on its own. `main.cpp` is the working half in isolation, compiled and run to show the value starting at its compile-time-resolved value and then being reassigned without any complaint. `fail_storage.cpp` is a separate, narrower failure mode not shown in the post's own snippet, `constinit` applied to an ordinary local variable, which is rejected regardless of whether the initializer is a compile-time constant, since the restriction is about storage duration, not just initializer resolvability.

## Key insight

```cpp
int get_runtime_value() { return 69; }         // not constexpr
constexpr int get_compile_time_value() { return 69; }

constinit int bad  = get_runtime_value();       // fails to compile
constinit int good = get_compile_time_value();  // compiles fine

int main() {
    good = 100;   // legal, constinit only locks the start
}
```

Same keyword, two completely different concerns, when the value is allowed to be decided, and whether it can change afterward.

## Run: fail.cpp

```bash
g++ -O2 -std=c++26 -c fail.cpp -o fail.o
```
This is expected to fail. Expect two errors on `bad`, one naming the variable itself as lacking a constant initializer, one naming the specific function call responsible.

## Run: main.cpp

```bash
g++ -O2 -std=c++26 main.cpp -o main && ./main
```
Expect the program to print `69` first, then `100`, confirming the compile-time-resolved starting value and the legal mutation afterward.

## Run: fail_storage.cpp

```bash
g++ -O2 -std=c++26 -c fail_storage.cpp -o fail_storage.o
```
This is expected to fail. Expect an error naming the storage duration restriction directly, not anything about the initializer, `local` never gets far enough to have its initializer checked.

## Output

```
$ g++ -O2 -std=c++26 -c fail.cpp -o fail.o
fail.cpp:14:15: error: 'constinit' variable 'bad' does not have a constant initializer
   14 | constinit int bad  = get_runtime_value();
      |               ^~~
fail.cpp:14:39: error: call to non-'constexpr' function 'int get_runtime_value()'
   14 | constinit int bad  = get_runtime_value();
      |                      ~~~~~~~~~~~~~~~~~^~
fail.cpp:11:5: note: 'int get_runtime_value()' declared here
   11 | int get_runtime_value() { return 69; }
      |     ^~~~~~~~~~~~~~~~~
```
Two errors on `bad` plus a note pointing back at the offending function's own declaration. The first error names the variable, the second names the specific reason.

```
$ g++ -O2 -std=c++26 main.cpp -o main && ./main
before: 69
after: 100
```
Starts at the compile-time-resolved value, then mutates freely, exactly the two separate promises this entry is about.

```
$ g++ -O2 -std=c++26 -c fail_storage.cpp -o fail_storage.o
fail_storage.cpp: In function 'void foo()':
fail_storage.cpp:12:19: error: 'constinit' can only be applied to a variable with static or thread storage duration
   12 |     constinit int local = get_value();
      |                   ^~~~~
```
A single, direct error about storage duration, `local` never gets far enough for its initializer to even be checked.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 16.1.0
