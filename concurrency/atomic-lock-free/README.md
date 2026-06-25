# std::atomic and Lock-Free Guarantees

Full writeup: https://valay17.github.io/Portfolio/blog/concurrency/atomic-lock-free

`std::atomic<T>` only guarantees atomicity. Whether the implementation is lock-free is left up to the platform and the size of the type. Most platforms make `atomic<integral-type>` and `atomic<T*>` lock-free, but that is a platform choice, not a language promise. The only types with a hard, portable guarantee of lock-free behavior are `atomic_flag`, `atomic_signed_lock_free`, and `atomic_unsigned_lock_free`. For anything else, `std::atomic<T>::is_always_lock_free` has to be checked, not assumed.

The reason comes down to hardware. A CPU can atomically touch a fixed number of bytes in one instruction, typically up to 8 on x86_64, sometimes 16 with specific instructions. Wrap a struct bigger than that in `std::atomic` and there is no single instruction wide enough to update it all at once. The implementation has to fall back to something else under the hood, usually a mutex, because the hardware itself has no atomic primitive that wide.

## What the code demonstrates

`lock-free-check.cpp` checks `is_always_lock_free` correctly, as a static member of the type rather than an instance member, against a 4 byte struct that fits in one instruction, a 32 byte struct that does not, and the three types the standard guarantees lock-free behavior for regardless of platform.

`lock-free-instruction.cpp` is not run. It is compiled to assembly and inspected with objdump, comparing the store instructions generated for atomics of increasing size: 4, 8, 16, and 32 bytes. The smaller ones should compile to a single inline instruction. The 32 byte one should compile to a call into libatomic instead, since no x86_64 instruction can move that much data atomically in one step. This is where the size versus instruction width claim becomes visible directly in the generated code, rather than just in the true/false result from `is_always_lock_free`.

## Key insight

```cpp
// is_always_lock_free is static, it belongs to the type, not the object
std::atomic<Small>::is_always_lock_free   // correct
some_atomic_small.is_always_lock_free     // wrong, do not call it this way
```

```cpp
struct Small { int x; };          // 4 bytes, fits in one instruction
struct Big   { int data[8]; };    // 32 bytes, no single instruction covers it
```

## A separate, related risk: alignment

Size is the main reason `is_always_lock_free` can come back false. There is a second, narrower way to lose lock-free behavior even on a type that would normally qualify, and it does not show up through `is_always_lock_free` at all, because that check is purely about the type, not about any specific object's actual address at runtime.

Ordinary struct layout cannot misalign a `std::atomic<T>`. The compiler enforces `alignof` regardless of what surrounds it in a struct. The risk shows up specifically when code works with raw memory directly, for example placement-newing an atomic into a buffer obtained from a custom allocator, or reinterpreting a pointer into memory that was not allocated with that type's alignment in mind. If the resulting address is not properly aligned, an atomic operation that would otherwise compile to a single locked instruction on x86 can straddle a cache line boundary at runtime and fall back to a full bus lock instead, which stalls every core on the memory bus, not just the one doing the access. This is undefined behavior under the standard, not a documented fallback path, and it is a real risk specifically in code that manages its own memory rather than letting normal allocation handle placement.

This repo does not include a runnable demo for the alignment case. The size versus instruction width fallback above is the reliably observable one. The alignment risk is noted here because it is a related way the same general guarantee breaks down, worth knowing if working with custom allocators or raw buffers, not because it is easy to force reliably on this hardware.

## Run: lock-free-check.cpp

```bash
g++ -O2 -std=c++20 lock-free-check.cpp -o lock-free-check && ./lock-free-check
```
`-O2` is the standard optimization level used across this repo.

## Output

```
atomic<Small>::is_always_lock_free = true
atomic<Big>::is_always_lock_free = false
atomic_flag is always lock-free by the standard, no check needed
atomic_signed_lock_free::is_always_lock_free = true
atomic_unsigned_lock_free::is_always_lock_free = true
```

## Run: lock-free-instruction.cpp

Compile to assembly, no execution:

```bash
g++ -O2 -std=c++20 -c lock-free-instruction.cpp -o lock-free-instruction.o
objdump -d -M intel --no-show-raw-insn lock-free-instruction.o
```
`-c` compiles to an object file without linking, since this file has no main and is not meant to run. `objdump -d` disassembles the compiled object file into assembly. `-M intel` selects Intel syntax over the default AT&T syntax, generally easier to read for this kind of comparison. `--no-show-raw-insn` hides the raw instruction bytes, leaving just the mnemonics.

## objdump output

```asm
0000000000000000 <_Z7store_4v>:
   0:   mov    rax,QWORD PTR [rip+0x0]
   7:   mov    DWORD PTR [rax],0x0      <-- single inline store, no call
   d:   xor    eax,eax
   f:   ret

0000000000000010 <_Z7store_8v>:
  10:   mov    rax,QWORD PTR [rip+0x0]
  17:   mov    QWORD PTR [rax],0x0      <-- single inline store, no call
  1e:   xor    eax,eax
  20:   ret

0000000000000030 <_Z8store_16v>:
  30:   mov    rdi,QWORD PTR [rip+0x0]
  37:   xor    ecx,ecx
  39:   xor    esi,esi
  3b:   xor    edx,edx
  3d:   jmp    42 <_Z8store_16v+0x12>   <-- jumps into a helper, not a plain inline mov like store_4/8

0000000000000050 <_Z8store_32v>:
  50:   push   rbp
  51:   pxor   xmm0,xmm0
  55:   xor    ecx,ecx
  57:   mov    edi,0x20
  5c:   mov    rbp,rsp
  5f:   sub    rsp,0x30
  63:   mov    rsi,QWORD PTR [rip+0x0]
  79:   lea    rdx,[rbp-0x30]
  7d:   movaps XMMWORD PTR [rbp-0x30],xmm0
  81:   movaps XMMWORD PTR [rbp-0x20],xmm0
  85:   call   8a <_Z8store_32v+0x3a>   <-- this is the libatomic call, the actual fallback
  99:   leave
  9a:   ret

```
Stack protector instructions and `nop` alignment padding between functions are trimmed from the output above, they are compiler safety and alignment artifacts unrelated to the atomic mechanism itself.

`store_4` and `store_8` compile to one plain `mov`, no call involved, since 8 bytes or fewer fits in a single x86_64 instruction. `store_16` and `store_32` both hand off to libatomic instead, `store_16` with a tail jump and `store_32` with a real call, since neither size fits in one instruction. See the Godbolt output below for the actual function names.

## Godbolt

Compiled and linked at `-O2`, x86-64 gcc, confirms the fallback function names directly:

```asm
store_4():
        mov     DWORD PTR a4[rip], 0
        ret
store_8():
        mov     QWORD PTR a8[rip], 0
        ret
store_16():
        xor     ecx, ecx
        xor     esi, esi
        xor     edx, edx
        mov     edi, OFFSET FLAT:a16
        jmp     __atomic_store_16       <-- jumps into a helper, not a plain inline mov like store_4/8
store_32():
        sub     rsp, 40
        pxor    xmm0, xmm0
        xor     ecx, ecx
        mov     esi, OFFSET FLAT:a32
        mov     rdx, rsp
        mov     edi, 32
        movaps  XMMWORD PTR [rsp], xmm0
        movaps  XMMWORD PTR [rsp+16], xmm0
        call    __atomic_store          <-- this is the libatomic call, the actual fallback
        add     rsp, 40
        ret
```

`__atomic_store_16` and `__atomic_store` are libatomic functions, the GCC runtime library backing atomics the hardware cannot handle in one instruction.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 14.3.0
