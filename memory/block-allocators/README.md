# Block Allocators and Memory Fragmentation

Full writeup: https://valay17.github.io/Portfolio/blog/memory/block-allocators

A program can have 100 MB of free memory and still have to grow by 400 MB to satisfy its next allocation. The free memory isn't gone. It's in pieces too small to use.

`malloc` carves every block out of one shared region, the heap. `free` hands a block back, but its neighbors stay where they are. Free a block that has live blocks next to it and you're left with a hole that can only hold something no bigger than itself. Holes can't merge while live objects sit between them, so the total free bytes can be enough while no single hole is big enough. That's fragmentation.

A block allocator (also called a pool allocator) avoids it by never mixing sizes. It reserves one region up front and cuts it into equal chunks, each the size of one object. Freed chunks sit on a list. Allocating takes a chunk off the list and freeing puts it back, with no search. A free chunk stores the pointer to the next free chunk inside itself, so the list costs no extra memory, and a chunk that is in use carries no bookkeeping. The cost is one pool per object size.

## What the code demonstrates

Four files, one per claim.

`fragmentation.cpp` sets up 100,000 connections, each a 1 KB scratch buffer followed by a 64-byte session object, then frees every scratch buffer and allocates 100,000 new buffers of a size given on the command line. At 1 KB the new buffers reuse the holes and the process doesn't grow. At 4 KB none fit a hole and the process grows by about the full cost of the buffers.

`carving.cpp` repeats the setup and frees the scratch buffers, then optionally allocates 100,000 small 200-byte objects before a second wave of 1 KB buffers. With `malloc`, the small objects take pieces out of the holes and the second wave has to grow the process. With the scratch buffers in a pool, the second wave still finds every chunk whole.

`stride.cpp` prints how far apart two consecutive 16-byte `malloc` blocks land and how many bytes `malloc` reports as usable, which shows the bookkeeping each block carries.

`pool.cpp` is the `PoolAllocator` class from the post with a small driver that checks the free list: four chunks come out distinct, a fifth request returns `nullptr`, and a freed chunk is the next one handed back.

## Key insight

```cpp
PoolAllocator scratch_pool(1024, 100000);   // one region, cut into 100,000 chunks of 1 KB

for (int i = 0; i < 100000; ++i) {
    scratch[i] = scratch_pool.allocate();   // pop a chunk, no search
    session[i] = malloc(sizeof(Session));   // lives as long as the connection
}

for (int i = 0; i < 100000; ++i)
    scratch_pool.deallocate(scratch[i]);    // chunks go back on the free list, still whole

scratch[0] = scratch_pool.allocate();       // the next scratch buffer gets a whole 1 KB chunk
```

Nothing else can carve a pool's chunks, so every freed chunk fits the next scratch buffer. A pool built for one size can't hand out another, which is the trade.

## Run: fragmentation.cpp

```bash
g++ -std=c++20 -O2 fragmentation.cpp -o fragmentation
./fragmentation 1024
./fragmentation 4096
```
`-O2` is the standard optimization level used across this repo. The argument is the size of the second round of buffers. The program reads resident memory (RSS) from `/proc/self/statm` after setup, after freeing the scratch buffers, and after the new buffers are allocated and written.

```
next buffers of 1024 B: RSS after setup 115264 KB, after freeing scratch 115388 KB, after 100000 new buffers 115388 KB (grew 0 KB, 0 MB)
next buffers of 4096 B: RSS after setup 115328 KB, after freeing scratch 115452 KB, after 100000 new buffers 517012 KB (grew 401560 KB, 392 MB)
```
Freeing the scratch buffers doesn't lower RSS, since the 100 MB of holes stays with the process. The 1 KB buffers drop into those holes and the process doesn't grow at all. The 4 KB buffers fit none of them, so each one needs fresh memory, and the growth of 392 MB is about the full cost of 100,000 x 4 KB.

## Run: carving.cpp

```bash
g++ -std=c++20 -O2 carving.cpp -o carving
./carving malloc 0
./carving malloc 1
./carving pool 1
```
The first argument picks where the 1 KB scratch buffers come from, `malloc` or a `PoolAllocator`. The second is `1` to allocate the 200-byte small objects between the two waves and `0` to skip them.

```
malloc smalls=0 | after freeing scratch 116140 KB | after smalls 116264 KB (+124) | after 2nd wave of 1 KB 116268 KB (+4 KB = 0 MB)
malloc smalls=1 | after freeing scratch 116140 KB | after smalls 116264 KB (+124) | after 2nd wave of 1 KB 136580 KB (+20316 KB = 19 MB)
pool   smalls=1 | after freeing scratch 114580 KB | after smalls 135016 KB (+20436) | after 2nd wave of 1 KB 135016 KB (+0 KB = 0 MB)
```
With no small objects in between, the second wave reuses the holes and the process doesn't grow. With them, the small objects settle into the holes first and the second wave grows the process by 19 MB. With the scratch buffers in a pool, the second wave grows it by 0 KB, because the small objects have no holes to settle into and every chunk is still whole. The pool doesn't shrink the total here: the small objects grow the heap by 20 MB instead, and the final RSS is about the same, 136580 KB with `malloc` and 135016 KB with the pool.

## Run: stride.cpp

```bash
g++ -std=c++20 -O2 stride.cpp -o stride
./stride
```

```
malloc(16): addresses differ by 32 bytes, usable size 24
```
Two consecutive 16-byte blocks sit 32 bytes apart and `malloc` reports 24 usable bytes in the first, so each block carries bookkeeping beyond what was requested. A pool chunk in use carries none, since the free-list pointer lives inside the chunk only while it is free.

## Run: pool.cpp

```bash
g++ -std=c++20 -O2 pool.cpp -o pool
./pool
```

```
fifth is null: 1, distinct: 1
freed chunk reused: 1
```
A pool of four chunks hands out four distinct chunks and returns `nullptr` for the fifth. The chunk freed back is the next one handed out.

## Scope

Only the default `malloc` was tested. Other allocators such as jemalloc and tcmalloc, other request sizes near the hole size, and network servers were not. The scenario is the allocation sequence a server would make, not a measured server.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 16.1.0
