# Memory Fences

Full writeup: https://valay17.github.io/Portfolio/blog/concurrency/memory-fences

A memory fence is not a hardware switch. It is an instruction to the
compiler about what it is allowed to reorder. On x86, the hardware is
already strongly ordered for most operations, so a fence's job is mostly
done at compile time, by stopping the compiler from moving things around.
seq_cst is the exception. It needs an actual CPU instruction to enforce a
global order across cores, because that guarantee is stronger than
anything the hardware gives you by default.

`atomic_thread_fence` detaches the wall from any single atomic variable
and applies it standalone in the code, ordering everything around it,
atomics and regular memory access included. A standalone fence does
nothing on its own. It needs an atomic operation somewhere nearby to have
any effect at all. No atomic involved, no synchronization happens, fence
or not.

`atomic_signal_fence` looks similar but solves a different problem
entirely. It orders memory access between a thread and a signal handler
running on that same thread, not between two separate threads. No CPU
instructions involved here either, purely a compiler-level reordering
barrier.

## What the code demonstrates

Two files here, each showing a different side of this.

`dekker.cpp` forces the fence's effect to become observable. Two threads
each set their own plain bool flag, then check the other thread's flag
before entering a critical section. Without any fence, the compiler and
CPU are free to reorder "set my flag" after "read your flag", which lets
both threads conclude the other has not started yet, and both enter the
critical section at once. With a fence between the flag write and the
flag read, paired with a dummy atomic operation since a fence needs one
nearby to have any effect, that reordering is prevented. Each mode runs as
its own process invocation. Note that this is Dekker-flavored, not Dekker's
actual algorithm, the real algorithm involves a wait loop rather than a
single check, simplified here to make violations easier to trigger and
count.

`fence-codegen.cpp` is not meant to be run. It is meant to be compiled to
assembly and inspected with objdump, to see directly that a relaxed atomic
store compiles to a plain instruction with no fence involved, while adding
a standalone seq_cst fence after it introduces extra instructions, because
seq_cst is the one ordering that costs something at the hardware level.

## Key insight

```cpp
flag_a = true;
// nothing stops the compiler from moving the read of flag_b
// above this line, with no fence and no atomic involved
if (!flag_b) { /* wrongly believes it has exclusive access */ }
```

```cpp
flag_a = true;
dummy_atomic.fetch_add(0, std::memory_order_relaxed); // gives the fence something to attach to
std::atomic_thread_fence(std::memory_order_seq_cst);  // the actual wall
if (!flag_b) { /* now correctly ordered after the write to flag_a */ }
```

## Run: dekker.cpp

Compile once:

```bash
g++ -O2 -std=c++20 -pthread dekker.cpp -o dekker
```
`-O2` keeps the build representative of real code, since an unoptimized
build can mask or change reordering behavior on its own.
`-pthread` is required on Linux with GCC whenever `std::thread` is used.

Run each mode as its own process, separately:

```bash
./dekker no-fence
./dekker with-fence
```
Running these separately, rather than back to back in one process, keeps
thread scheduling behavior from one run from influencing the other.

Each run reports how many times both threads entered the critical section
together, and what the unprotected shared counter ended up at. If mutual
exclusion held throughout, the counter should equal the iteration count
exactly. Any other value, or any nonzero "both entered together" count, is
evidence the reordering happened.

This is a data race in the no-fence case, the shared counter
increment is unprotected. ThreadSanitizer will correctly flag this one.

## Run: fence-codegen.cpp

Compile to assembly, no execution:

```bash
g++ -O2 -std=c++20 -c fence-codegen.cpp -o fence-codegen.o
objdump -d --no-show-raw-insn fence-codegen.o
```
`-c` compiles to an object file without linking or producing a runnable
binary, since this file has no main and is not meant to run.
`objdump -d` disassembles the compiled object file into assembly.
`--no-show-raw-insn` hides the raw instruction bytes, leaving just the
mnemonics, easier to read for this comparison.

Look for the disassembly of `no_fence()` and `with_seq_cst_fence()`
separately in the output. Expect `no_fence` to show a single store
instruction with nothing extra around it. Expect `with_seq_cst_fence` to
show the same store, plus additional instructions after it, typically a
locked instruction or an explicit fence instruction, since seq_cst is the
ordering that actually costs a real CPU instruction on x86, unlike
relaxed, acquire, or release.

## Output
```
dekker.cpp no-fence:
no-fence: both threads entered together 28 times out of 2000000 iterations
no-fence: shared_counter ended at 2000000 (expected 2000000 if mutual exclusion held)

dekker.cpp with-fence:
with-fence: both threads entered together 0 times out of 2000000 iterations
with-fence: shared_counter ended at 1999989 (expected 2000000 if mutual exclusion held)

```

```
objdump disassembly no_fence:
0000000000000000 <_Z8no_fencev>:
   0:   mov    0x0(%rip),%rax        # 7 <_Z8no_fencev+0x7>
   7:   movl   $0x1,(%rax)
   d:   xor    %eax,%eax
   f:   ret

objdump disassembly with_seq_cst_fence:
0000000000000010 <_Z18with_seq_cst_fencev>:
  10:   mov    0x0(%rip),%rax        # 17 <_Z18with_seq_cst_fencev+0x7>
  17:   movl   $0x1,(%rax)
  1d:   lock orq $0x0,(%rsp)         <-- Notice this instruction here
  23:   xor    %eax,%eax
  25:   ret

```
Info on the lock x86 instruction: it is not an instruction itself, it is an instruction prefix, which applies to its following instruction. It is applied to something that does a read-modify-write(RMW) on memory. 

The LOCK prefix ensures that the CPU has exclusive ownership of the appropriate cache line for the duration of the operation, and provides certain additional ordering guarantees. This may be achieved by asserting a bus lock, but the CPU will avoid this where possible. If the bus is locked then it is only for the duration of the locked instruction.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 14.3.0
