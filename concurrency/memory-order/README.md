# Acquire/Release, Sequentially_Consistent Memory Ordering

Full writeup: https://valay17.github.io/Portfolio/blog/concurrency/memory-order

A write that happens earlier in your code can still be seen out of order
by a different thread. Acquire and release orderings on atomics create a
one way wall between exactly two threads, the ones touching the same
atomic variable. A release store says everything written before this point
is visible to any thread that does a matching acquire load. The acquire
load says anything read after this point can see everything that happened
before the release. seq_cst extends that same wall to every thread in the
program, agreeing on one global order. Relaxed has no wall at all, the
operation is atomic but free to be reordered.

## What the code demonstrates

Two files, each a standalone three thread example. One writer thread
stores into two separate atomics, x and y. Two reader threads each read
both atomics, in opposite orders.

`acquire-release.cpp` uses release on the stores and acquire on the loads.
The wall this creates only exists between the writer and each individual
atomic. It says nothing about whether reader1 and reader2 agree on the
relative order x and y became visible, because x and y are two separate
walls, not one. This file runs the three thread setup up to 2 million
times, stopping the moment it finds a run where reader1 and reader2 imply
a different order for x and y, and printing that run's values. If no such
run shows up across all 2 million attempts, it reports that instead.

`seq-cst.cpp` is the same three threads, same stores and loads, with
every operation changed to seq_cst. This places every operation into one
global order all threads agree on, including operations on different
atomics. Reader1 and reader2 cannot end up implying two different orders
for x and y becoming visible. This file runs once and prints what each
reader saw.

Neither file is a timing benchmark. The printed values are the
demonstration.

## Key insight

```cpp
// release (write side)
data = 69;
ready.store(true, std::memory_order_release);
// nothing written before this line can be moved below it

// acquire (read side)
while (!ready.load(std::memory_order_acquire));
print(data); // guaranteed to see 69
// nothing read after this line can be moved above it
```

The wall only exists between threads touching the same atomic. seq_cst is
the same wall, applied globally, so every thread agrees on one order even
for atomics they never touch.

## What you might see on this machine

x86 already provides strong ordering at the hardware level for ordinary
loads and stores. Run `acquire-release.cpp` and on this CPU it will most
likely report no disagreement found even after 2 million attempts, even
though acquire/release alone does not technically forbid that
disagreement from happening.

That is not the demo failing. It is the actual, well known reason subtly
wrong memory ordering can run correctly for years on x86 and then break
the moment the same code runs on ARM or another weakly ordered
architecture. The guarantee acquire/release does not give you is still
absent. This machine's hardware just happens to provide a stronger
guarantee underneath it anyway, which is not something your code is
allowed to rely on.

Read both files as: this is what the C++ standard does and does not
promise, not as: this is what will visibly break on this run. The
difference between them is in what the language guarantees, not
necessarily in what you will observe on this specific machine.

## Run

Compile each example separately:

```bash
g++ -O2 -std=c++20 -pthread acquire-release.cpp -o acquire-release
g++ -O2 -std=c++20 -pthread seq-cst.cpp -o seq-cst
```
`-O2` is the standard optimization level used across this repo.
`-pthread` is required on Linux with GCC whenever `std::thread` is used.

Run each one on its own:

```bash
./acquire-release
./seq-cst
```

`acquire-release` may take a while to finish since it spins up three
threads per attempt, up to 2 million attempts, looking for a disagreement
between the two readers. If a disagreement is found, it stops immediately
and prints that run. If not, it reports that no disagreement was found
after exhausting all attempts. Consider testing with a smaller
`kMaxIterations` first to get a sense of how long a full run takes on this
machine before committing to the full 2 million.

`seq-cst` runs once and prints what each reader saw immediately.

## Output

```
acquire-release:

No disagreement found in 2000000 runs. See the README for why this is the expected result on x86, not evidence that the ordering guarantee exists.

seq_cst:

reader1: saw x = 1
reader1: saw y = 1
reader2: saw y = 1
reader2: saw x = 1

With seq_cst, reader1 and reader2 are placed on the same global timeline as the writer's stores to x and y. They cannot disagree about the relative order x and y became visible, the way acquire/release alone permitted.
```

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 14.3.0
