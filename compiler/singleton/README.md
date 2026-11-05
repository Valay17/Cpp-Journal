# The Singleton Guard and What constexpr Removes

Full writeup: https://valay17.github.io/Portfolio/blog/compiler/singleton-guard

Every call to a singleton's `instance()` checks a hidden guard. Give the constructor a `constexpr` and the check disappears.

The Singleton pattern means one object for the whole program, reached through a function. In modern C++ it's a `static` variable inside that function, and the function returns a reference to it. Since C++11 the standard guarantees that is thread-safe, and the compiler pays for the guarantee. It adds a hidden flag next to the object, and every call loads the flag, tests it and branches. Only the first call takes the slow path. It calls into the runtime library (`__cxa_guard_acquire`) to take a lock, builds the object, then releases the lock.

While one thread is building it, every other thread that calls the function waits. After that the check is cheap, but it's still a load and a branch on every access.

Make the constructor `constexpr` and the compiler builds the object at compile time. There's no flag and no lock, and `instance()` shrinks to one instruction that hands back the address. This only works when the constructor can run at compile time. One that reads a file or calls other code still needs the guard. Writing `constinit` on the variable makes the compiler refuse to compile if the constructor can't be evaluated that early.

## What the code demonstrates

Four files, one per claim.

`codegen.cpp` holds the post's two structs, identical except for the `constexpr` on `Const`'s constructor. It is not run. It is compiled to an object file and inspected with `objdump` and `nm`, which shows the guard check and the calls into the runtime library in `plain_instance` and nothing but an address in `const_instance`.

`timing.cpp` has four threads call `instance()` at the same moment. The constructor spins for 300 ms, and each thread records how long its call took in wall time and in CPU time. The same four threads then call an `instance()` with a `constexpr` constructor.

`constinit_ok.cpp` puts `constinit` on the `constexpr` version, which compiles. `constinit_fail.cpp` puts it on the non-`constexpr` version, which does not.

## Key insight

```cpp
struct Plain {
    int value;
    Plain() : value(69) {}              // same body, not constexpr
};

Plain& plain_instance() {
    static Plain c;                     // guard: load, test, branch on every call
    return c;                           // first call: __cxa_guard_acquire, build, release
}

struct Const {
    int value;
    constexpr Const() : value(69) {}    // same body, constexpr
};

Const& const_instance() {
    static Const c;                     // no guard: built at compile time
    return c;
}
```

The two constructors have the same body. The only difference is one keyword, and it decides whether every call pays for a guard.

## Run: codegen.cpp

```bash
g++ -std=c++20 -O2 -c codegen.cpp -o codegen.o
objdump -dr -M intel --no-show-raw-insn --demangle codegen.o | awk '/<plain_instance\(\)>:/,/^$/'
objdump -dr -M intel --no-show-raw-insn --demangle codegen.o | awk '/<const_instance\(\)>:/,/^$/'
```
`-O2` is the standard optimization level used across this repo, and `-c` compiles to an object file without linking, since this file has no `main`. In `objdump`, `-d` disassembles, `-r` prints the relocations so the names of the functions being called show up, `-M intel` selects Intel syntax, `--no-show-raw-insn` hides the instruction bytes and `--demangle` turns the symbol names back into readable C++. The `awk` keeps one function at a time, from its label to the next blank line.

```
0000000000000000 <plain_instance()>:
  0:   movzx  eax,BYTE PTR [rip+0x0]        # 7 <plain_instance()+0x7>
                       3: R_X86_64_PC32        .bss-0x4
  7:   test   al,al
  9:   je     18 <plain_instance()+0x18>
  b:   lea    rax,[rip+0x0]        # 12 <plain_instance()+0x12>
                       e: R_X86_64_PC32        .bss+0x4
 12:   ret
 13:   nop    DWORD PTR [rax+rax*1+0x0]
 18:   push   rbp
 19:   lea    rdi,[rip+0x0]        # 20 <plain_instance()+0x20>
                       1c: R_X86_64_PC32       .bss-0x4
 20:   mov    rbp,rsp
 23:   call   28 <plain_instance()+0x28>
                       24: R_X86_64_PLT32      __cxa_guard_acquire-0x4
 28:   test   eax,eax
 2a:   jne    38 <plain_instance()+0x38>
 2c:   lea    rax,[rip+0x0]        # 33 <plain_instance()+0x33>
                       2f: R_X86_64_PC32       .bss+0x4
 33:   pop    rbp
 34:   ret
 35:   nop    DWORD PTR [rax]
 38:   lea    rdi,[rip+0x0]        # 3f <plain_instance()+0x3f>
                       3b: R_X86_64_PC32       .bss-0x4
 3f:   mov    DWORD PTR [rip+0x0],0x45        # 49 <plain_instance()+0x49>
                       41: R_X86_64_PC32       .bss
 49:   call   4e <plain_instance()+0x4e>
                       4a: R_X86_64_PLT32      __cxa_guard_release-0x4
 4e:   lea    rax,[rip+0x0]        # 55 <plain_instance()+0x55>
                       51: R_X86_64_PC32       .bss+0x4
 55:   pop    rbp
 56:   ret
 57:   nop    WORD PTR [rax+rax*1+0x0]

0000000000000060 <const_instance()>:
 60:   lea    rax,[rip+0x0]        # 67 <const_instance()+0x7>
                       63: R_X86_64_PC32       .data-0x4
 67:   ret
```
`plain_instance` starts by loading one byte, the guard flag, from `.bss` (`movzx`), testing it (`test`) and jumping to the slow path if it is zero (`je`). Every call after the first falls through to the `lea` and `ret` at offsets `b` and `12`, which return the address of the object, so the steady-state cost is that load, test and branch. The slow path starts at offset `18`. It calls `__cxa_guard_acquire`, stores `0x45` (69) into the object, which is the constructor body inlined, and calls `__cxa_guard_release`. The `push rbp` and `mov rbp,rsp` on the slow path are frame pointer setup from this machine's default compiler flags and are not part of the guard. `const_instance` is a `lea` and a `ret`. It loads no flag, tests nothing and calls nothing.

```bash
nm -C codegen.o
objdump -s -j .data codegen.o
```
`nm` lists the symbols in the object file, with `-C` demangling them, and `b` marks `.bss` while `d` marks `.data`. `objdump -s -j .data` dumps the contents of the `.data` section.

```
                U __cxa_guard_acquire
                U __cxa_guard_release
0000000000000060 T const_instance()
0000000000000000 T plain_instance()
0000000000000000 b guard variable for plain_instance()::c
0000000000000000 d const_instance()::c
0000000000000008 b plain_instance()::c

codegen.o:     file format elf64-x86-64

Contents of section .data:
0000 45000000                             E...
```
The compiler generated a `guard variable` for `plain_instance()::c` and none for `const_instance()::c`. The plain object lives in `.bss`, which holds zeroes until the program writes to it at run time. The `constexpr` object lives in `.data` and already contains `45000000`, which is 69 as a 4-byte integer, stored in the file before the program starts. That is what "built at compile time" means here.

## Run: timing.cpp

```bash
g++ -std=c++20 -O2 -pthread timing.cpp -o timing
./timing
```
`-pthread` is required for `std::thread` with GCC on Linux. Each thread waits on a flag so all four call `instance()` at the same moment, then records its wall time (how long the call took) and its CPU time (how long the thread was running during it).

```
constructor that takes 300 ms (guarded):
 thread 0: wall  300.02 ms, cpu  299.99 ms
 thread 1: wall  300.36 ms, cpu    0.03 ms
 thread 2: wall  300.06 ms, cpu    0.03 ms
 thread 3: wall  300.42 ms, cpu    0.03 ms
constexpr constructor (no guard):
 thread 0: wall    0.00 ms, cpu    0.00 ms
 thread 1: wall    0.00 ms, cpu    0.00 ms
 thread 2: wall    0.00 ms, cpu    0.00 ms
 thread 3: wall    0.00 ms, cpu    0.00 ms
```
All four threads spent 300 ms inside the call. Thread 0 ran the constructor and used 299.99 ms of CPU doing it. The other three used 0.03 ms of CPU across their 300 ms, so they were asleep waiting for the guard to be released. Which thread runs the constructor changes from run to run. With a `constexpr` constructor there is nothing to wait for and every call takes no measurable time.

## Run: constinit_ok.cpp and constinit_fail.cpp

```bash
g++ -std=c++20 -O2 constinit_ok.cpp -o constinit_ok && ./constinit_ok; echo $?
g++ -std=c++20 -O2 constinit_fail.cpp -o constinit_fail
```
`echo $?` prints the program's exit code, and the program returns 0 when the value it reads is 69.

```
0
```
`constinit_ok.cpp` compiles and exits with 0. `constinit` adds no behavior to a variable that is already constant-initialized. It makes the compiler check that it is.

```
constinit_fail.cpp: In function ‘Plain& plain_instance()’:
constinit_fail.cpp:13:28: error: ‘constinit’ variable ‘c’ does not have a constant initializer
  13 |     static constinit Plain c;
     |                            ^
constinit_fail.cpp:13:28: error: call to non-‘constexpr’ function ‘Plain::Plain()’
constinit_fail.cpp:9:5: note: ‘Plain::Plain()’ declared here
   9 |     Plain() : value(69) {}
     |     ^~~~~
```
`Plain`'s constructor cannot run at compile time, so the variable needs a guard, and `constinit` turns that into a compile error naming the constructor instead of a guard that quietly stays on every call.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 16.1.0
