# Out-of-Order Execution: What the Compiler Does With It

Full writeup: https://valay17.github.io/Portfolio/blog/cpu/out-of-order-execution

Your CPU doesn't run your code in the order you wrote it. It never promised to.

Waiting for one instruction to fully finish before starting the next would leave most of the CPU idle most of the time. So the CPU keeps fetching ahead, and when it hits a branch, it doesn't stop to find out which way things go, it guesses using a branch predictor, and keeps executing down that guessed path while the correct answer is still being worked out. A wrong guess throws away everything executed down the wrong path, a pipeline flush, and the CPU restarts from the correct point.

But branches are just the visible symptom. The actual rule underneath is broader: the CPU runs instructions as soon as their inputs are ready, not in the order they sit in the source. The compiler plays the same game one level up, reordering instructions at compile time whenever it can prove the single-threaded result won't change. Hardware and compiler, two separate layers, both rearranging code, and both required to make it look like nothing was rearranged at all, as long as only one thread is watching.

## What the code demonstrates

One file, two functions, not run, compiled and disassembled only. Both compute an independent value `b` alongside a value `a` that depends on something slow, then return `a + b`. `b` never depends on `a`, matching the post's own snippet exactly. The only difference between the two functions is what "slow" actually is.

`compute_call` gets `a` from a call to an external, non-inlined function. `compute_load` gets it from a plain array load instead. Compiling both and reading the assembly shows the compiler treats these completely differently, and the reason comes down to something more fundamental than instruction scheduling: a `call` redirects control flow. Instructions physically located after a `call` in the binary are not fetched at all until the call actually returns, there is no instruction stream for the compiler or the CPU to reorder ahead of it. A load is not a control-flow instruction, so both the compiler statically and the CPU dynamically remain free to schedule other work around it.

This means the post's own snippet, `long a = slow_function(x);`, only demonstrates the reordering it describes if `slow_function` is read as a stand-in for something like a slow, cache-missing load, not a literal call to a separate function. Worth being precise about, since the two look almost identical in source but compile to structurally different code.

## Key insight

```cpp
long compute_call(long x, long y, long z, long w) {
    long a = slow_function(x);                              // a call, a hard boundary
    long b = (y * 2 + 7) ^ (z * 3 - w) + (y & z) * (w | y);  // independent of a
    return a + b;
}

long compute_load(const long* arr, long x, long y, long z, long w) {
    long a = arr[x];                                          // a load, not a boundary
    long b = (y * 2 + 7) ^ (z * 3 - w) + (y & z) * (w | y);  // same expression
    return a + b;
}
```

Identical `b` expression in both functions. The only variable is whether `a` comes from a call or a load.

## Run: codegen.cpp

```bash
g++ -O2 -std=c++20 -c codegen.cpp -o codegen.o
objdump -d -M intel --no-show-raw-insn codegen.o | grep -A 30 "compute_call"
```
`-c` compiles to an object file without linking, since this file has no `main` and is not meant to run. `-M intel` selects Intel syntax. `--no-show-raw-insn` hides the raw instruction bytes. `grep -A 30` isolates just this function and the instructions after it, through to `ret`. Expect every instruction computing `b` to appear after the `call`, not before it, since nothing after a call is fetched until the call returns.

```bash
objdump -d -M intel --no-show-raw-insn codegen.o | grep -A 12 "compute_load"
```
Same idea, isolating the second function, `-A 12` since this one is shorter. Expect the opposite of `compute_call`, every instruction computing `b` should appear before the load of `a`, with the load itself as one of the last instructions before the return, likely fused directly with the final addition.

## Output

```
$ objdump -d -M intel --no-show-raw-insn codegen.o

0000000000000000 <compute_call(long, long, long, long)>:
   0:   push   rbp
   1:   mov    rbp,rsp
   4:   push   r13
   6:   mov    r13,rcx
   9:   push   r12
   b:   mov    r12,rdx
   e:   push   rbx
   f:   mov    rbx,rsi
  12:   sub    rsp,0x8
  16:   call   1b <compute_call(long, long, long, long)+0x1b>   <-- a requested here
  1b:   mov    rdx,rbx
  1e:   add    rsp,0x8
  22:   mov    rcx,rax
  25:   mov    rax,rbx
  28:   or     rdx,r13
  2b:   and    rax,r12
  2e:   imul   rax,rdx
  32:   lea    rdx,[r12+r12*2]
  36:   sub    rdx,r13
  39:   add    rax,rdx
  3c:   lea    rdx,[rbx+rbx*1+0x7]                             <-- all of b computed after the call
  41:   pop    rbx
  42:   pop    r12
  44:   xor    rax,rdx
  47:   pop    r13
  49:   pop    rbp
  4a:   add    rax,rcx                                          <-- a + b
  4d:   ret

0000000000000050 <compute_load(long const*, long, long, long, long)>:
  50:   mov    rax,rdx
  53:   mov    r9,rdx
  56:   lea    rdx,[rdx+rdx*1+0x7]
  5b:   and    rax,rcx
  5e:   or     r9,r8
  61:   lea    rcx,[rcx+rcx*2]
  65:   imul   rax,r9
  69:   sub    rcx,r8
  6c:   add    rax,rcx
  6f:   xor    rax,rdx                                          <-- b fully computed here, before any load
  72:   add    rax,QWORD PTR [rdi+rsi*8]                         <-- a loaded and added to b in one instruction
  76:   ret
```
`compute_call` has nothing computing `b` before the `call` at `0x16`, only register saves to keep `y`, `z`, `w` alive across it. Every instruction building `b` runs after the call returns. `compute_load` has the entire `b` expression finished by `0x6f`, before the load at `0x72` ever runs, and that load is fused directly with the final addition rather than sitting in its own instruction. Same source-level shape, same independent `b`, structurally different generated code, entirely because one of them crosses a call boundary and the other doesn't.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 16.1.0
