# SIMD: One Instruction, a Batch of Values

Full writeup: https://valay17.github.io/Portfolio/blog/cpu/simd

A normal instruction processes one value: load one, add one, store one. SIMD (single instruction multiple data) loads a batch of values into one wide register and applies the same operation to the whole batch at once. Four floats added to four floats, or eight, or sixteen, in the same instruction that used to handle one. Not a faster core, not a shortcut, the instruction itself works on a batch instead of a single value.

This is a different thing from superscalar execution. Superscalar is a CPU running several separate instructions in one cycle, each doing its own job in a different execution unit, more instructions at once. SIMD is one instruction doing more, a single instruction operating on a batch of values instead of one. A CPU does both simultaneously, but they solve different problems.

Register width sets the batch size. SSE (Streaming SIMD Extensions) gives 128 bits, four floats at a time. AVX (Advanced Vector Extensions) doubles that to 256 bits, eight floats. AVX-512 doubles it again, sixteen. Same add, wider register, more values per instruction, nothing about the math changes.

Compilers can generate this automatically, auto-vectorization, but it is never guaranteed on a given loop, confirmed directly below. The alternative to hoping the compiler does it is writing it explicitly with intrinsics, the instruction written is the instruction emitted, no compiler discretion involved.

## What the code demonstrates

Two files, and the second one took debugging to get right, which turned out to be worth documenting rather than hiding.

`main.cpp` is `add_normal` against `add_simd`, one add per element. Compiled and disassembled, not benchmarked. Timing it directly showed this exact operation is memory-bandwidth-bound, one add for every float loaded and stored means the CPU spends nearly all its time waiting on memory, not computing, so normal and SIMD landed within noise of each other at every array size tested. The instruction-level story stands on its own, one `vaddps` replaces eight `vaddss`, it just does not translate into a timed difference for an operation this simple. This file is where the codegen difference lives, not the wall-clock number.

`benchmark.cpp` is a compute-heavier version built to get a timed number, since `main.cpp`'s simple add cannot produce one.

`compute_normal` runs four independent chains across four different elements, `i`, `i+1`, `i+2`, `i+3`, instead of one chain per element. A single chain here is a serial dependency, `x = x*y + a[i]` depends on its own previous result, so the CPU cannot start step `k+1` before step `k` resolves, and there is nothing else independent for it to work on while it waits. This is a question of instruction-level parallelism (ILP), how many independent instructions a CPU can have in flight and overlapping at once. Four independent chains give it four streams to interleave instead of one.

`compute_simd` is the same idea with a single vector chain, eight elements per step instead of one. Compared against `compute_normal`, this isolates the ILP advantage more than the width advantage, since `compute_normal` already has four-way ILP and this has one chain, giving a reproducible ~2x.

`compute_simd4` gives the vector version the same four-way treatment, four independent `__m256` chains instead of one. With ILP equal on both sides, the remaining gap against `compute_normal` isolates the actual vector-width advantage, close to the theoretical 8x ceiling for 256-bit AVX.

## Key insight

```cpp
std::vector<float> add_normal(const std::vector<float>& a, const std::vector<float>& b) {
    std::vector<float> out(a.size());
    for (size_t i = 0; i < a.size(); i++)
        out[i] = a[i] + b[i];
    return out;
}

std::vector<float> add_simd(const std::vector<float>& a, const std::vector<float>& b) {
    std::vector<float> out(a.size());
    size_t i = 0;
    for (; i + 8 <= a.size(); i += 8) {
        __m256 va = _mm256_loadu_ps(&a[i]);
        __m256 vb = _mm256_loadu_ps(&b[i]);
        __m256 vsum = _mm256_add_ps(va, vb);
        _mm256_storeu_ps(&out[i], vsum);
    }
    for (; i < a.size(); i++)
        out[i] = a[i] + b[i];
    return out;
}
```

`add_simd` processes eight floats per loop iteration instead of one, using AVX intrinsics directly instead of hoping the compiler's auto-vectorizer recognizes the loop. Whether that difference shows up as a timed speedup depends entirely on whether the operation is compute-bound or memory-bound, confirmed by `benchmark.cpp` below.

## Run: main.cpp

```bash
g++ -O2 -std=c++20 -march=native main.cpp -o main && ./main
```
`-march=native` targets the CPU actually running the build, required here since `<immintrin.h>`'s AVX intrinsics do not compile at all without an AVX-enabled target.

## Run: codegen, default build

```bash
g++ -O2 -std=c++20 -march=native -c main.cpp -o main-o2.o
objdump -d -M intel --no-show-raw-insn main-o2.o | grep -A 15 "add_normal\|add_simd"
```
`-c` compiles to an object file without linking, since this is only about inspecting the generated instructions. `grep -A 15` keeps each matched function and the 15 lines after it, cutting the startup and library glue code objdump otherwise prints. Look at `add_normal`. Expect a scalar `vaddss`, one float per instruction, no auto-vectorization at `-O2`.

Look at `add_simd` in the same output. Expect `vaddps` with a `ymm` register, the 8-wide AVX add, present here without needing `-O3`, since intrinsics map directly to the instruction written regardless of optimization level.

## Run: codegen, -O3 build

```bash
g++ -O3 -std=c++20 -march=native -c main.cpp -o main-o3.o
objdump -d -M intel --no-show-raw-insn main-o3.o | grep -A 40 "add_normal"
```
Look at `add_normal` here specifically, `-A 40` since the vectorized version is longer than the scalar one. Expect `vaddps` with a `ymm` register this time, auto-vectorization applied at `-O3` where it did not at `-O2`. Also expect more than one version of the loop in the disassembly, GCC generates a runtime check for whether `a` and `b` overlap in memory before it can safely use the vectorized path, since the function takes two separate references and cannot prove at compile time that they do not alias.

## Run: vectorizer report

```bash
g++ -O2 -std=c++20 -march=native -fopt-info-vec-optimized -c main.cpp -o /dev/null
g++ -O3 -std=c++20 -march=native -fopt-info-vec-optimized -c main.cpp -o /dev/null
```
`-fopt-info-vec-optimized` makes the compiler report every loop it successfully vectorized directly in its own words, rather than inferring it from disassembly. Expect no mention of `add_normal`'s loop at `-O2`, and a line naming it directly at `-O3`, along with a note that the loop was versioned for aliasing.

## Run: benchmark.cpp

```bash
g++ -O2 -std=c++20 -march=native benchmark.cpp -o benchmark
./benchmark normal
./benchmark simd
./benchmark simd4
```
Run each mode as its own separate invocation rather than timing multiple modes in one combined run, so no result is biased by whatever another mode left warmed up in cache. `normal` is the four-way ILP scalar version. `simd` is a single eight-wide vector chain, compare this against `normal` to see the ILP-only gap. `simd4` is four independent eight-wide vector chains, compare this against `normal` to see the width-only gap once ILP is equal on both sides. Watch the printed checksum, it should be identical across all three modes, confirming every version computed the same answer regardless of how the arithmetic was organized.

```
$ ./benchmark normal
normal: 3466099 us, checksum=1.17644e+09

$ ./benchmark simd
simd: 1738350 us, checksum=1.17644e+09

$ ./benchmark simd4
simd4: 433877 us, checksum=1.17644e+09
```
`normal` vs `simd`: 3466099 / 1738350 ≈ 1.99x, the ILP-only comparison. `normal` vs `simd4`: 3466099 / 433877 ≈ 7.99x, the width-only comparison, essentially the full theoretical ceiling for 256-bit AVX.

## Output

```
$ ./main
outputs match: yes
```

```
$ g++ -O2 -std=c++20 -march=native -fopt-info-vec-optimized -c main.cpp -o /dev/null
[gcc/include/c++/bits/stl_vector.h]:106:4: optimized: basic block part vectorized using 16 byte vectors
```
At `-O2`, the only vectorization reported anywhere is inside `std::vector`'s own internals, its zero-initializing constructor. Nothing mentions `add_normal`'s loop or `add_simd`'s tail loop, confirming neither is auto-vectorized at this level.

```
$ g++ -O3 -std=c++20 -march=native -fopt-info-vec-optimized -c main.cpp -o /dev/null
[gcc/include/c++/bits/stl_vector.h]:106:4: optimized: basic block part vectorized using 16 byte vectors
main.cpp:7:26: optimized: loop vectorized using 32 byte vectors and unroll factor 8
main.cpp:7:26: optimized:  loop versioned for vectorization because of possible aliasing
main.cpp:7:26: optimized: epilogue loop vectorized using 16 byte vectors and unroll factor 4
main.cpp:21:14: optimized: loop vectorized using 16 byte vectors and unroll factor 4
main.cpp:21:14: optimized:  loop versioned for vectorization because of possible aliasing
main.cpp:21:14: optimized: epilogue loop vectorized using 8 byte vectors and unroll factor 2
[gcc/include/c++/bits/stl_algobase.h]:1196:20: optimized: loop vectorized using 32 byte vectors and unroll factor 8
[gcc/include/c++/bits/stl_algobase.h]:1196:20: optimized:  loop versioned for vectorization to enhance alignment
```
At `-O3`, `main.cpp:7:26`, `add_normal`'s loop, is vectorized directly, matching the disassembly, with a runtime alias check inserted first. A bonus finding here too: `main.cpp:21:14`, the scalar tail loop inside `add_simd` that handles whatever is left over after processing full batches of 8, gets auto-vectorized on its own at `-O3` as well, narrower, 16-byte vectors instead of 32, since it is only ever handling at most 7 leftover elements.

```
$ objdump -d -M intel --no-show-raw-insn main-o2.o | grep -A 15 "add_normal\|add_simd"

<add_normal(...)>:
  ...
  c0:   vmovss xmm0,DWORD PTR [rsi+rax*4]
  c5:   vaddss xmm0,xmm0,DWORD PTR [rdi+rax*4]   <-- scalar, one float per instruction
  ca:   vmovss DWORD PTR [rcx+rax*4],xmm0
  cf:   inc    rax
  d2:   cmp    rax,rdx
  d5:   jb     c0

<add_simd(...)>:
  ...
 204:   vmovups ymm0,YMMWORD PTR [rsi+rcx*1]
 210:   vaddps ymm0,ymm0,YMMWORD PTR [rax+rcx*1]   <-- 8-wide AVX add, present at -O2
 219:   vmovups YMMWORD PTR [rax+rcx*1],ymm0
  ...
 260:   vmovss xmm0,DWORD PTR [rsi+rdx*4]
 265:   vaddss xmm0,xmm0,DWORD PTR [rdi+rdx*4]   <-- scalar tail loop, unvectorized at -O2
 26a:   vmovss DWORD PTR [rcx+rdx*4],xmm0
```
`add_normal` shows only one arithmetic sequence in the entire function, `vaddss`, scalar, no `ymm` register anywhere. `add_simd` shows `vaddps` on a `ymm` register in its main loop at `-O2` already, confirming intrinsics do not need `-O3`, and its own leftover tail loop, handling whatever does not divide evenly into 8, is still plain scalar `vaddss` here, matching the vectorizer report's earlier finding that this tail loop stays unvectorized at `-O2`.

```
$ objdump -d -M intel --no-show-raw-insn main-o3.o | grep -A 40 "add_normal"

<add_normal(...)>:
  ...
 120:   vmovups ymm0,YMMWORD PTR [rsi+rax*1]
 125:   vaddps ymm0,ymm0,YMMWORD PTR [rdx+rax*1]   <-- main vectorized path, 8-wide
 12a:   vmovups YMMWORD PTR [rcx+rax*1],ymm0
  ...
 15c:   vmovups xmm0,XMMWORD PTR [rsi+rax*4]
 164:   vaddps xmm0,xmm0,XMMWORD PTR [rdx+rax*4]   <-- epilogue, 4-wide, leftover after the 8-wide chunks
 171:   vmovups XMMWORD PTR [rcx+rax*4],xmm0
  ...
 220:   vmovss xmm0,DWORD PTR [rdx+rax*4]
 225:   vaddss xmm0,xmm0,DWORD PTR [rsi+rax*4]   <-- scalar fallback, taken if the alias check fails
 22a:   vmovss DWORD PTR [rcx+rax*4],xmm0
```
Three separate paths in one function now. The main path is the 8-wide `ymm` vectorized loop the vectorizer report named directly. The 4-wide `xmm` block is the epilogue handling whatever is left after full 8-wide chunks are exhausted, matching the report's "epilogue loop vectorized using 16 byte vectors" line exactly. The plain scalar block is the fallback path taken only if the runtime alias check between `a` and `b` fails, confirming the "loop versioned for vectorization because of possible aliasing" line describes a code path that actually exists in the binary, not just a note in the report.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 16.1.0
