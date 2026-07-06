# RVO and NRVO

Full writeup: https://valay17.github.io/Portfolio/blog/compiler/rvo-nrvo

Adding `std::move` to a return statement looks like an optimization. It is actually the opposite. When you return a local variable directly, the compiler constructs it in place at the call site, no copy, no move, zero overhead. This is Return Value Optimization (RVO). The moment you write `return std::move(a)`, you hand the compiler a complex expression instead of a plain local variable. RVO requires a plain named local or a prvalue. `std::move` produces neither. The optimization is gone, and now you are paying for a move constructor call that should never have existed.

RVO comes in two forms. Unnamed RVO, returning a prvalue directly, has been mandatory since C++17, the compiler cannot skip it. Named Return Value Optimization (NRVO), returning a named local variable, is optional but all major compilers apply it in the straightforward case. The distinction matters because NRVO can be defeated, unnamed RVO cannot.


## What the code demonstrates

`main.cpp` shows the difference at runtime through constructor call output. `MyObject` prints which constructor fires so the difference between `make_rvo` and `make_no_rvo` is visible without reading assembly. `make_rvo` returns a named local directly, NRVO applies, and only the constructor fires. `make_no_rvo` wraps the return in `std::move`, NRVO is defeated, and a move constructor call appears that should never have existed.

`codegen.cpp` is not run. It is compiled to assembly and inspected with objdump, confirming the same result at the instruction level: `make_rvo` generates a single `puts` call, `make_no_rvo` generates two.

## Key insight

```cpp
MyObject make_rvo() {
    MyObject a;
    return a;            // NRVO applies, construct in place, no move
}

MyObject make_no_rvo() {
    MyObject a;
    return std::move(a); // NRVO defeated, move fires unnecessarily
}
```

The fix is always to return the local variable directly. The compiler knows better than the programmer here, and adding `std::move` is not a hint, it is an obstacle.

## Run: main.cpp

```bash
g++ -O2 -std=c++20 main.cpp -o main && ./main
```
`-O2` is the standard optimization level used across this repo.

```
=== make_rvo ===
construct
=== make_no_rvo ===
construct
move
```
`make_rvo` prints one line, the constructor and nothing else, confirming NRVO applied and the object was built directly in place. `make_no_rvo` prints the constructor followed by a move, the exact call that `std::move` introduced and that should not exist.

## Run: -Wpessimizing-move

`-Wpessimizing-move` has existed since GCC 9 and ships as part of `-Wall`, no separate flag needed in a normal build. It fires specifically when a return statement wraps a local variable in `std::move`, which is the exact anti-pattern this repo is about.

`-Wnrvo` (added in GCC 14) is a different warning and does not catch this case. It warns when NRVO was eligible under the standard's elision rules, a plain `return a;` naming an automatic variable, but the compiler's own analysis still could not perform it, usually from complicated control flow. `return std::move(a);` never reaches that check at all, because the moment `std::move` is added, the return statement's operand is a call expression, not a bare name, and the standard's elision rule requires the operand to be the name itself. `-Wnrvo` has nothing to say about a case that was never a candidate in the first place, so it produced no output here, correctly, not as a missed detection. `-Wpessimizing-move` is the flag that actually targets this specific mistake, and it has for five years longer than `-Wnrvo` has existed.

```bash
g++ -O2 -std=c++20 -Wpessimizing-move main.cpp -o main
```
`-Wpessimizing-move` is a warning flag, not an optimization flag, so it does not change codegen, it only reports the `std::move` in the return statement. This also fires with plain `-Wall`, since `-Wpessimizing-move` is included in it by default.

```
main.cpp: In function 'MyObject make_no_rvo()':
main.cpp:34:21: warning: moving a local object in a return statement prevents copy elision [-Wpessimizing-move]
   34 |     return std::move(a); // NRVO defeated: move fires unnecessarily
      |            ~~~~~~~~~^~~
main.cpp:34:21: note: remove 'std::move' call
```
The warning points directly at the `return std::move(a);` line, and the compiler's own note tells you the fix, remove the `std::move` call. Nothing is reported for `make_rvo`, since there is nothing wrong with it.

## Run: codegen.cpp

```bash
g++ -O2 -std=c++20 -c codegen.cpp -o codegen.o
objdump -d -M intel --no-show-raw-insn codegen.o | grep -A 10 "make_rvo\|make_no_rvo"
```
`-c` compiles to an object file without linking, since this file has no main and is not meant to run. `-M intel` selects Intel syntax. `--no-show-raw-insn` hides the raw instruction bytes. The `grep` keeps both functions and the instructions immediately following each label.

```
0000000000000000 <make_rvo()>:
   0:   push   rbp
   1:   mov    rbp,rsp
   4:   push   rbx
   5:   mov    rbx,rdi
   8:   lea    rdi,[rip+0x0]        # f <make_rvo()+0xf>
   f:   sub    rsp,0x8
  13:   call   18 <make_rvo()+0x18>   <-- one call, construct only
  18:   mov    rax,rbx
  1b:   mov    rbx,QWORD PTR [rbp-0x8]
  1f:   leave
  20:   ret

0000000000000030 <make_no_rvo()>:
  30:   push   rbp
  31:   mov    rbp,rsp
  34:   push   rbx
  35:   mov    rbx,rdi
  38:   lea    rdi,[rip+0x0]        # 3f <make_no_rvo()+0xf>
  3f:   sub    rsp,0x8
  43:   call   48 <make_no_rvo()+0x18>   <-- construct
  48:   lea    rdi,[rip+0x0]        # 4f <make_no_rvo()+0x1f>
  4f:   call   54 <make_no_rvo()+0x24>   <-- move, should not exist
  54:   mov    rax,rbx
  57:   mov    rbx,QWORD PTR [rbp-0x8]
  5b:   leave
  5c:   ret
```
`make_rvo` has one `call` in its body, the constructor. `make_no_rvo` has two, the constructor and a second call for the move that `std::move` forced into existence. Same shape as the runtime output above, confirmed at the instruction level.

## Godbolt output

Compiled at `-O2`, x86-64 gcc, confirms the instruction-level difference directly.
```asm
make_rvo():
    call puts        ; construct only, NRVO applied
    ret

make_no_rvo():
    call puts        ; construct
    call puts        ; move, this call should not exist
    ret
```

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 16.1.0
