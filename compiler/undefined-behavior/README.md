# Undefined Behavior: Two Checks the Compiler Is Allowed to Delete

Full writeup: https://valay17.github.io/Portfolio/blog/compiler/undefined-behavior

Two small functions, two different kinds of undefined behavior, same underlying mechanism. One checks for signed integer overflow after the fact. The other checks for a null pointer after dereferencing it. In both cases the check exists to catch something the standard says cannot legally happen, and in both cases the compiler is free to use that guarantee against the check itself, sometimes deleting it outright, sometimes trapping the entire call path instead of running it.

This is not a compiler bug in either case. It is the compiler using exactly the freedom the standard grants it.

## Example 1: the disappearing overflow check

```cpp
bool will_overflow(int x) {
    if (x + 1 < x) {
        return true;
    }
    return false;
}
```

Signed integer overflow is undefined behavior, not implementation-defined, not guaranteed to wrap. The standard says `x + 1` overflowing cannot happen in a correct program, so the compiler assumes `x + 1` is always greater than `x`, and the condition is always false.

**Default build**: at both `-O0` and `-O2`, the check is eliminated entirely, `will_overflow` becomes an unconditional `return false`. The instruction-level detail is below, next to the disassembly.

**-fwrapv build**: not the same as the default build, at both `-O0` and `-O2`. With `-fwrapv`, overflow is defined to wrap using two's complement instead of being undefined, so `x + 1 < x` is no longer impossible, it is true exactly when `x == INT_MAX`. The default build deletes the check because it can prove it is dead. The `-fwrapv` build keeps it because it is not dead, it is just narrower than the source's literal `x + 1 < x` computation.

**UBSan build**: the diagnostic fires at both `-O0` and `-O2`. It was initially missed because UBSan writes to stderr while the program's own output goes to stdout, and UBSan defaults to recover mode, logging and continuing rather than stopping.

## Example 2: the check placed after the dereference

```cpp
int read_value(int* ptr) {
    int val = *ptr;
    if (ptr == nullptr) {
        return -1;
    }
    return val;
}
```

Dereferencing a null pointer is undefined behavior. The bug here is not that the check is missing, it is that the check runs after the dereference in source order. The compiler is free to assume the dereference never involves a null pointer, since that is the only way the operation would be legal, so it can treat the check afterward as dead.

**Default build**: at `-O0`, the check survives but is placed too late to matter. At `-O2`, the check disappears entirely and `main`, which calls this with a literal `nullptr`, gets something more aggressive than a deletion. Detail below, next to the disassembly.

**-fno-delete-null-pointer-checks build**: at `-O0`, identical to the default build, this flag only affects what the optimizer is allowed to assume, and `-O0` does not reason about it either way. At `-O2`, the check survives. Whether that actually fixes anything is addressed next to that disassembly, since the answer is not as clean as it looks.

**UBSan build**: fires at both `-O0` and `-O2`, with different outcomes, explained next to each output.

## Run: overflow example, default build

```bash
g++ -O0 -std=c++20 -c overflow-ub.cpp -o overflow-ub-o0.o
g++ -O2 -std=c++20 -c overflow-ub.cpp -o overflow-ub-o2.o
objdump -d -M intel --no-show-raw-insn overflow-ub-o0.o | grep -A 15 "will_overflow"
objdump -d -M intel --no-show-raw-insn overflow-ub-o2.o | grep -A 10 "will_overflow"
```
`-c` compiles to an object file without linking. `objdump -d` disassembles. `-M intel` selects Intel syntax over the default AT&T syntax. `--no-show-raw-insn` hides the raw instruction bytes. `grep -A` keeps only the relevant function and a fixed number of lines after it.

```
-O0 default build:

0000000000000000 <will_overflow(int)>:
   0:   push   rbp
   1:   mov    rbp,rsp
   4:   mov    DWORD PTR [rbp-0x4],edi   <-- x stored, never read again
   7:   mov    eax,0x0                   <-- unconditional false
   c:   pop    rbp
   d:   ret
```
`x` is stored to the stack (`mov DWORD PTR [rbp-0x4],edi`) but never read again. The next line sets the return value to `0` unconditionally. Even at `-O0`, normally the level with the least reasoning applied, the compiler had already folded the check to constant false before the argument was ever used for anything.

```
-O2 default build:

0000000000000000 <will_overflow(int)>:
   0:   xor    eax,eax                   <-- unconditional false
   2:   ret
```
Same result as `-O0`, just in its most compact form. `xor eax,eax` is the standard idiom for zeroing a register cheaply, this is "return false" with no parameter read at all.

## Run: overflow example, -fwrapv build

```bash
g++ -O0 -std=c++20 -fwrapv -c overflow-ub.cpp -o overflow-ub-o0-wrapv.o
g++ -O2 -std=c++20 -fwrapv -c overflow-ub.cpp -o overflow-ub-o2-wrapv.o
objdump -d -M intel --no-show-raw-insn overflow-ub-o0-wrapv.o | grep -A 15 "will_overflow"
objdump -d -M intel --no-show-raw-insn overflow-ub-o2-wrapv.o | grep -A 10 "will_overflow"
```
`-fwrapv` tells the compiler signed overflow wraps using two's complement instead of being undefined, an explicit contract instead of the standard's default rule.

```
-O0 -fwrapv build:

0000000000000000 <will_overflow(int)>:
   0:   push   rbp
   1:   mov    rbp,rsp
   4:   mov    DWORD PTR [rbp-0x4],edi
   7:   cmp    DWORD PTR [rbp-0x4],0x7fffffff   <-- comparison against INT_MAX
   e:   jne    17 <will_overflow(int)+0x17>
  10:   mov    eax,0x1
  15:   jmp    1c <will_overflow(int)+0x1c>
  17:   mov    eax,0x0
  1c:   pop    rbp
  1d:   ret
```
The source's literal `x + 1 < x` is not translated step by step even here. Instead of computing `x + 1` and comparing it to `x`, the compiler already reduces the whole condition to a direct comparison against `INT_MAX` (`0x7fffffff`), because under wraparound semantics that comparison is the exact condition, `x + 1 < x` is true if and only if `x == INT_MAX`.

```
-O2 -fwrapv build:

0000000000000000 <will_overflow(int)>:
   0:   cmp    edi,0x7fffffff               <-- comparison against INT_MAX
   6:   sete   al                           <-- sets return value from that comparison
   9:   ret
```
Same comparison as `-O0`, in its compact form. `sete al` is the x86 instruction for "set byte if equal", it writes `1` into the low byte of the return register if the preceding `cmp` found the two values equal, `0` otherwise, the branchless way to turn a comparison directly into a boolean return value.

## Run: overflow example, UBSan build

```bash
g++ -O0 -std=c++20 -fsanitize=undefined overflow-ub.cpp -o overflow-ub-o0
g++ -O2 -std=c++20 -fsanitize=undefined overflow-ub.cpp -o overflow-ub-o2
./overflow-ub-o0 2>&1
./overflow-ub-o2 2>&1
```
`-fsanitize=undefined` links in UBSan, which instruments operations to detect undefined behavior at runtime. UBSan writes diagnostics to stderr, so `2>&1` merges it with stdout in this run.

```
$ ./overflow-ub-o0 2>&1
overflow-ub.cpp:38:11: runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
1
```
The diagnostic fires even at `-O0`, on stderr. The program keeps running afterward, prints `1`, and exits normally, since UBSan's default mode logs the violation and continues rather than stopping.

```
$ ./overflow-ub-o2 2>&1
overflow-ub.cpp:38:11: runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
1
```
Same diagnostic, same line and column, this time at `-O2`. The sanitizer's instrumentation fires independently of optimization level, even though the surrounding code shape in the non-sanitized builds above is completely different between `-O0` and `-O2`.

## Run: nullptr example, default build

```bash
g++ -O0 -std=c++20 -c nullptr-ub.cpp -o nullptr-ub-o0.o
g++ -O2 -std=c++20 -c nullptr-ub.cpp -o nullptr-ub-o2.o
objdump -d -M intel --no-show-raw-insn nullptr-ub-o0.o | grep -A 20 "read_value"
objdump -d -M intel --no-show-raw-insn nullptr-ub-o2.o | grep -A 15 "read_value"
```

```
-O0 default build:

0000000000000000 <read_value(int*)>:
   0:   push   rbp
   1:   mov    rbp,rsp
   4:   mov    QWORD PTR [rbp-0x18],rdi
   8:   mov    rax,QWORD PTR [rbp-0x18]
   c:   mov    eax,DWORD PTR [rax]          <-- dereference happens first
   e:   mov    DWORD PTR [rbp-0x4],eax
  11:   cmp    QWORD PTR [rbp-0x18],0x0      <-- null check happens after
  16:   jne    1f <read_value(int*)+0x1f>
  18:   mov    eax,0xffffffff
  1d:   jmp    22 <read_value(int*)+0x22>
  1f:   mov    eax,DWORD PTR [rbp-0x4]
  22:   pop    rbp
  23:   ret
```
Line `c` dereferences `ptr` and stores the result. Line `11` checks `ptr` against null only after that dereference already happened, a literal translation of the source, check included but too late to matter when `ptr` is null.

```
-O2 default build:

0000000000000000 <read_value(int*)>:
   0:   mov    eax,DWORD PTR [rdi]          <-- check is gone entirely
   2:   ret

main, inlined and folded (default build, -O2):

0000000000000000 <main>:
   0:   mov    eax,DWORD PTR ds:0x0
   7:   ud2                                 <-- provably UB, trap instead of run
```
`read_value` on its own loses the check entirely, it is now an unconditional dereference of whatever pointer it is given. The interesting part is `main`. It calls this with a literal `nullptr`, so the compiler can see, at compile time, that the only way this call executes is by dereferencing address `0`, which is undefined behavior. Rather than generate the literal dereference or anything that computes a plausible-looking answer, GCC emits `ud2`, "undefined instruction 2", an x86 instruction whose entire purpose is to be illegal. Executing it raises an invalid-opcode exception and the OS delivers `SIGILL`, killing the process immediately. This is different from a plain null dereference, which would fault with `SIGSEGV` from an actual attempted memory access. With `ud2`, the CPU never attempts the memory access at all, it refuses to execute the instruction itself, because the compiler already proved this code path can never legally run.

## Run: nullptr example, -fno-delete-null-pointer-checks build

```bash
g++ -O0 -std=c++20 -fno-delete-null-pointer-checks -c nullptr-ub.cpp -o nullptr-ub-o0-fnull.o
g++ -O2 -std=c++20 -fno-delete-null-pointer-checks -c nullptr-ub.cpp -o nullptr-ub-o2-fnull.o
objdump -d -M intel --no-show-raw-insn nullptr-ub-o0-fnull.o | grep -A 20 "read_value"
objdump -d -M intel --no-show-raw-insn nullptr-ub-o2-fnull.o | grep -A 15 "read_value"
```
`-fno-delete-null-pointer-checks` tells the compiler not to use "this pointer is never null" as a fact it can optimize with.

```
-O0 -fno-delete-null-pointer-checks build:

(identical to the -O0 default build above)
```
At `-O0` the optimizer is not reasoning about pointer nullness either way, so this flag has nothing to change.

```
-O2 -fno-delete-null-pointer-checks build:

0000000000000000 <read_value(int*)>:
   0:   mov    eax,DWORD PTR [rdi]          <-- still dereferences first
   2:   test   rdi,rdi                      <-- check preserved, but too late
   5:   je     8 <read_value(int*)+0x8>
   7:   ret
   8:   mov    eax,0xffffffff
   d:   ret
```
The check is back, `test rdi,rdi` followed by `je` is a null check. But look at the order: line `0` still dereferences `[rdi]` before line `2` tests whether `rdi` is null. This flag stops the compiler from deleting the check, it does not move the check earlier than the dereference, because that ordering came from the source, not from the optimizer. Called with a pointer whose value is not known until runtime, this build would still dereference before checking and still crash on a null input, flag or no flag. The fix for that is checking before dereferencing in the source, not this compiler flag.

## Run: nullptr example, UBSan build

```bash
g++ -O0 -std=c++20 -fsanitize=undefined nullptr-ub.cpp -o nullptr-ub-o0
g++ -O2 -std=c++20 -fsanitize=undefined nullptr-ub.cpp -o nullptr-ub-o2
./nullptr-ub-o0 2>&1
./nullptr-ub-o2 2>&1
```

```
$ ./nullptr-ub-o0 2>&1
nullptr-ub.cpp:32:9: runtime error: load of null pointer of type 'int'
Segmentation fault (core dumped)
```
The diagnostic fires, then the process still crashes. Recover mode logged the violation and returned from the sanitizer's own runtime call, but at `-O0` nothing gets reordered or folded, so the actual `mov eax, [address 0]` instruction the source scheduled is still the next thing that executes, and that is a hardware fault regardless of what the sanitizer already reported. Recover mode recovers from the sanitizer's own bookkeeping, not from the underlying operation still sitting in the instruction stream.

```
$ ./nullptr-ub-o2 2>&1
nullptr-ub.cpp:32:9: runtime error: load of null pointer of type 'int'
-1
```
Same diagnostic, but this time the process exits cleanly with `-1`. `main` passes a literal `nullptr`, so the same constant-folding that let the `-fno-delete-null-pointer-checks` build avoid a crash applies here too, the compiler proves the answer through the preserved, instrumented check without ever reaching the unsafe load. The unsafe load is dead code in this specific case, not something recover mode protected against.

## Does the compiler need to know the value in advance?

Both examples above call with a literal value, `2147483647` for the overflow check, `nullptr` for the pointer example. That can look like the trick only works because the compiler already knows the answer. It does not work that way for the check itself. It does matter for one specific detail in the nullptr example.

### Run: overflow example, argument from the command line

```bash
g++ -O2 -std=c++20 -c overflow-ub-argv.cpp -o overflow-argv-o2.o
objdump -d -M intel --no-show-raw-insn overflow-argv-o2.o | grep -A 5 "will_overflow"
g++ -O2 -std=c++20 overflow-ub-argv.cpp -o overflow-argv
./overflow-argv 2147483647
```

```
0000000000000000 <will_overflow(int)>:
   0:   xor    eax,eax
   2:   ret
```
`x` here comes from the command line, unknown until the program runs. The check still collapses to the same unconditional false as the literal-argument version. The elimination is a proof that holds for every possible `x` under the assumption that `x + 1` cannot overflow, not a fact about one specific input.

```
$ ./overflow-argv 2147483647
0
```

### Run: nullptr example, pointer from the command line

```bash
g++ -O2 -std=c++20 -c nullptr-ub-argv.cpp -o nullptr-argv-o2.o
objdump -d -M intel --no-show-raw-insn nullptr-argv-o2.o | grep -A 15 "^0000000000000000 <main>"
objdump -d -M intel nullptr-argv-o2.o | grep -c "ud2"
g++ -O2 -std=c++20 nullptr-ub-argv.cpp -o nullptr-argv
./nullptr-argv 0
```

```
0000000000000000 <main>:
   0:   push   rbp
   1:   mov    rdi,QWORD PTR [rsi+0x8]
   5:   mov    edx,0xa
   a:   xor    esi,esi
   c:   mov    rbp,rsp
   f:   call   14 <main+0x14>
  14:   lea    rdi,[rip+0x0]        # 1b <main+0x1b>
  1b:   mov    esi,DWORD PTR [rax]    <-- dereference inlined directly, no check
  1d:   call   22 <main+0x22>
  22:   mov    edx,0x1
  27:   lea    rsi,[rip+0x0]        # 2e <main+0x2e>
  2e:   mov    rdi,rax
  31:   call   36 <main+0x36>
  36:   xor    eax,eax
  38:   pop    rbp

ud2 count: 0
```
With a pointer the compiler cannot resolve at compile time, the binary contains no `ud2` instruction anywhere. `main` inlines the already-checkless dereference directly at `1b`, no trap, no defensive instruction of any kind.

```
$ ./nullptr-argv 0
Segmentation fault (core dumped)
```
`SIGSEGV`, not `SIGILL`. The trap in the earlier example existed only because the compiler could prove, at compile time, that the one call in `main` was unconditionally null. Take that knowledge away and the missing check does not announce itself, it just crashes wherever the dereference happens to land.

### What this means for the two examples above

The check being gone generalizes to every input, that is what makes it dangerous. The `ud2` trap is the part that depends on compile-time knowledge, and losing that knowledge does not make the code safer, it just makes the failure quieter and further from the actual mistake.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 14.3.0
