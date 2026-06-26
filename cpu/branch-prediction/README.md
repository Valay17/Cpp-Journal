# Branch Prediction

Full writeup: https://valay17.github.io/Portfolio/blog/cpu/branch-prediction

When a CPU mispredicts a branch, it does not just pause and wait, it already ran the wrong instructions and now has to throw all of that work away. Modern CPUs do not wait to know which way a branch goes, the frontend predicts the outcome and speculatively executes down that path before the condition is even resolved. A correct guess means free work done early. A wrong guess means the pipeline gets flushed, everything computed on the wrong path gets discarded, and execution restarts from the correct branch. That flush is the actual cost, not the branch itself.

Prediction is not a guess made fresh every time. A branch target buffer tracks where branches have gone before, and a separate predictor learns patterns over repeated runs through the same code. Loops with a consistent direction get predicted well. Branches on unpredictable data get predicted close to a coin flip, though not always exactly at that ceiling, see the actual numbers below.

`[[likely]]` and `[[unlikely]]` look like they talk to the branch predictor directly. They do not. The predictor is pure hardware, learning purely from runtime behavior, nothing in source code reaches it. These attributes only tell the compiler how to lay out the generated code.

## What the code demonstrates

`branch-cost.cpp` isolates misprediction cost from cache and data effects, deliberately avoiding the usual sorted versus unsorted array example, since sorting changes access pattern implications in ways that tangle branch effects together with cache effects. Both modes allocate the same size array, fill it sequentially, and read it sequentially in the same order, so memory footprint and access pattern are identical between the two modes by construction. The only thing that differs is the value stored at each position, which controls which way the branch goes. Since cache behavior depends on access pattern and footprint, not on the values themselves, any timing difference between the two modes comes from branch prediction specifically, not memory.

`likely-codegen.cpp` is not run. It is compiled to assembly and inspected with objdump, comparing two versions of the same classify function: no attribute, and `[[likely]]` on the else branch. Each branch writes to a `volatile` sink, which stops the compiler from collapsing the if/else into branchless arithmetic, keeping a real conditional jump in the generated code so there is an actual layout to compare.

## Key insight

```cpp
volatile int sink;

int classify(int x) {
    if (x > 0) {
        sink = 1;
        return 1;
    } else [[likely]] {
        sink = 2;
        return -1;
    }
}
```

`[[likely]]` does not reach the branch predictor. It tells the compiler which path to lay out as the straight-line fallthrough and which to push out of line behind a jump, a layout choice that can help prediction indirectly, not an instruction to the hardware predictor itself. The `volatile` write is what keeps this a real branch in the generated code rather than something the compiler optimizes away entirely.

## Run: likely-codegen.cpp

```bash
g++ -O2 -std=c++20 -c likely-codegen.cpp -o likely-codegen.o
objdump -d -M intel --no-show-raw-insn likely-codegen.o
```
`-c` compiles to an object file without linking, since this file has no main and is not meant to run. `-M intel` selects Intel syntax over the default AT&T syntax. `--no-show-raw-insn` hides the raw instruction bytes.

## objdump output

```
Disassembly of section .text:

0000000000000000 <_Z15classify_likelyi>:
   0:   mov    rax,QWORD PTR [rip+0x0]
   7:   test   edi,edi
   9:   jg     20 <_Z15classify_likelyi+0x20>
   b:   mov    DWORD PTR [rax],0x2                  <-- sink = 2
  11:   mov    eax,0xffffffff
  16:   xor    edi,edi
  18:   ret
  20:   mov    DWORD PTR [rax],0x1                  <-- sink = 1
  26:   mov    eax,0x1
  2b:   xor    edi,edi
  2d:   ret

0000000000000030 <_Z16classify_no_hinti>:
  30:   mov    rax,QWORD PTR [rip+0x0]
  37:   test   edi,edi
  39:   jle    50 <_Z16classify_no_hinti+0x20>
  3b:   mov    DWORD PTR [rax],0x1                  <-- sink = 1
  41:   mov    eax,0x1
  46:   xor    edi,edi
  48:   ret
  50:   mov    DWORD PTR [rax],0x2                  <-- sink = 2
  56:   mov    eax,0xffffffff
  5b:   xor    edi,edi
  5d:   ret
```

The volatile write does its job, both functions kept a real conditional jump, no branch elimination happened. The layout difference is the actual point. In `classify_likely`, the else branch carries `[[likely]]`, the `sink = 2; return -1;` path, and it lands immediately after the `test`, reached with no jump at all if `jg` is not taken. The unmarked `if` branch sits behind the explicit `jg` jump instead. In `classify_no_hint`, with no attribute anywhere, the layout flips: the `if` branch is now the one at the fallthrough position, and the else branch is the one reached through a jump. Moving the attribute from one branch to the other flipped which path got the cheap, no-jump position, directly confirming the attribute changes layout, not prediction.

## Run: branch-cost.cpp

```bash
g++ -O2 -std=c++20 branch-cost.cpp -o branch-cost
```
`-O2` is the standard optimization level used across this repo.

Run each mode as its own process, separately:

```bash
./branch-cost predictable
./branch-cost random
```
Running these separately, rather than back to back in one process, keeps any state from one run from influencing the other, the same separation used for every other good versus bad comparison in this repo.

```bash
perf stat -e branches,branch-misses,cycles,instructions ./branch-cost predictable
perf stat -e branches,branch-misses,cycles,instructions ./branch-cost random
```
`branches` and `branch-misses` together give the actual misprediction rate. `cycles` and `instructions` together give a sense of where cycles are going, though as the actual numbers below show, IPC alone does not isolate flush cost cleanly, wall clock time and the misprediction count itself tell the real story.

## Output and perf stat output

```
predictable: 26 ms, result = 100000000

 Performance counter stats for './branch-cost predictable':

       334,712,591      branches
         3,697,535      branch-misses
       524,107,788      cycles
     1,270,725,223      instructions

       0.122757276 seconds time elapsed

       0.073508000 seconds user
       0.049341000 seconds sys


random: 320 ms, result = 150006069

 Performance counter stats for './branch-cost random':

     1,172,798,377      branches
        54,882,138      branch-misses
     4,453,674,591      cycles
    11,126,222,247      instructions

       1.051614218 seconds time elapsed

       0.997629000 seconds user
       0.052032000 seconds sys
```

The misprediction rate makes the difference concrete. The predictable run mispredicts 3.7 million times out of 334.7 million branches, roughly 1.1 percent, close to the near-zero expected for a pattern the predictor learns fast. The random run mispredicts 54.9 million times out of 1.17 billion branches, roughly 4.7 percent, well below the 50 percent ceiling a true coin flip would produce, suggesting the predictor is still picking up some partial structure even on this input rather than failing completely. What matters more than the raw percentage is the absolute gap, 3.7 million mispredictions against 54.9 million, nearly 15 times as many flushes paid for in the random run.

IPC alone undersells the cost here. The predictable run retires roughly 2.42 instructions per cycle, the random run roughly 2.50, nearly identical, because IPC ends up dominated by the much larger instruction count in the random run rather than isolating flush cycles specifically. The real cost shows up in wall clock time instead: 26 ms against 320 ms, roughly 12 times slower for a loop doing the same shape of work on the same amount of data, lining up with the cycle counts, 524 million against 4.45 billion, roughly 8.5 times more total CPU work, almost entirely attributable to the far higher mispredict count rather than to any difference in the actual arithmetic performed.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 14.3.0
