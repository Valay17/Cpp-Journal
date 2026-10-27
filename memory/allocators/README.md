# reserve vs resize, and the Allocator Underneath

Full writeup: https://valay17.github.io/Portfolio/blog/memory/allocators

`reserve` doesn't construct anything. `resize` does. The difference is one call inside the allocator.

An allocator is the object a container asks for memory. Every allocator keeps two jobs apart. `allocate` hands back raw bytes, with no object alive in them yet. `construct` is a separate call that builds an object in one slot of those bytes.

That's why `reserve(100)` gives a vector a capacity of 100 and leaves its size at 0. The memory is there, but nothing has been built in it yet. `resize(100)` goes one step further and builds 100 objects, so the size becomes 100.

Because memory and objects are separate, the memory can come from anywhere. `std::allocate_shared` puts the `shared_ptr` bookkeeping and the object in one block, and `std::pmr::vector` (pmr is polymorphic memory resources) can take its bytes from a buffer on the stack.

## What the code demonstrates

Three files, one per claim group.

`reserve_resize.cpp` uses a small allocator that prints each `allocate` call and counts each `construct` call. `reserve(100)` produces one `allocate(100)` and zero constructs. `resize(100)` produces no new allocation and 100 constructs.

`shared.cpp` replaces the global `operator new` to count heap allocations and record their size. `allocate_shared<int>` makes one allocation, a 24-byte block holding the reference counts and the `int`. `shared_ptr<int>(new int)` makes two, the `int` and a separate control block.

`pmr.cpp` runs the same 100-element `push_back` loop on a plain `std::vector` and on a `std::pmr::vector` backed by a stack buffer, counting heap allocations in both.

## Key insight

```cpp
std::vector<int> v;
v.reserve(100);                  // capacity 100, size 0: bytes allocated, no ints built
v.resize(100);                   // size 100: 100 ints now built in those bytes

char buf[4096];
std::pmr::monotonic_buffer_resource res(buf, sizeof buf);
std::pmr::vector<int> p(&res);   // same vector, bytes come from the stack buffer

for (int i = 0; i < 100; ++i)
    p.push_back(i);              // 0 heap allocations
```

Same container, same code. Only the source of the bytes changed.

## Run: reserve_resize.cpp

```bash
g++ -std=c++20 -O2 reserve_resize.cpp -o reserve_resize
./reserve_resize
```
`-O2` is the standard optimization level used across this repo.

```
reserve(100):
  allocate(100)
  size=0 capacity=100 constructs=0
resize(100):
  size=100 capacity=100 constructs=100
```
`reserve(100)` calls `allocate` once for 100 slots and constructs nothing, so the size stays 0. `resize(100)` allocates nothing new, since the capacity already covers it, and calls `construct` 100 times, once per element, which is what moves the size to 100.

## Run: shared.cpp

```bash
g++ -std=c++20 -O2 shared.cpp -o shared
./shared
```

```
allocate_shared<int>:     allocations=1 block=24 bytes
shared_ptr<int>(new int): allocations=2
```
`allocate_shared<int>` asks its allocator for a single 24-byte block. The reference counts and the `int` share that block, and the `int` is built in a slice of it. `shared_ptr<int>(new int)` allocates the `int` first and then a separate control block, so it makes two allocations.

## Run: pmr.cpp

```bash
g++ -std=c++20 -O2 pmr.cpp -o pmr
./pmr
```

```
std::vector:      heap allocations=8
std::pmr::vector: heap allocations=0
```
The plain `std::vector` grows by doubling its capacity, 1, 2, 4 and so on up to 128, and each growth is a heap allocation, eight in total for 100 elements. The `std::pmr::vector` draws every one of those growths from the 4096-byte stack buffer, so nothing reaches the heap.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 16.1.0
