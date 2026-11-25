# A Stale Deque Iterator Works Until It Doesn't

Full writeup: https://valay17.github.io/Portfolio/blog/stl/iterator-invalidation

A stale deque iterator can read correct values for dozens of elements. Then the address sanitizer fires in the middle of the loop.

After elements are added at the end of a deque, its iterators are invalid. What happens next depends on how far you walk. A deque stores its elements in fixed-size chunks and tracks the chunks in an index. Adding at the end can reallocate that index and free the old one. A stale iterator still points into a chunk that exists, so reading and stepping inside that chunk touches nothing freed. At the chunk boundary, the increment reads the old index. That memory is gone, and the sanitizer reports a use-after-free.

A short loop passed and a longer one failed, with the same code and a stale iterator in both. Where it fails depends on the chunk size, not on the code.

A vector can fail more quietly. Erase the first element, and an iterator that pointed at the third now points at the fourth. No crash and no sanitizer report, just the wrong element. So a test with a small input can pass while production-sized data breaks. libstdc++'s checked mode (`-D_GLIBCXX_DEBUG`) reports both cases on the first use of the stale iterator, whatever the size.

## What the code demonstrates

Three files, one per claim.

`chunk.cpp` finds where a `deque<int>` changes chunks by looking for gaps between consecutive element addresses that are not `sizeof(int)`, which gives the number of ints per chunk.

`deque.cpp` takes an iterator to the first element, does 100,000 `push_back`s so the iterator is stale, then steps it a number of times given on the command line while summing the values. It prints the sum next to the sum read through a fresh iterator, so a wrong value would show up. It is run for 50 steps and for 200 steps.

`vector.cpp` takes an iterator to the third element of `{1, 2, 3, 4}`, erases the first element and reads through the iterator.

## Key insight

```cpp
std::deque<int> d(4, 1);
auto it = d.begin();                    // iterator to the first element
for (int i = 0; i < 100000; ++i)
    d.push_back(i);                     // chunk index reallocates, the old one is freed: it is now stale

// libstdc++ stores 128 ints per chunk (512 bytes), so the first chunk covers the first 128 elements
long sum = 0;
for (int k = 0; k < 50; ++k, ++it)      // 50 steps: stays inside the first chunk, correct values, no report
    sum += *it;
// for (int k = 0; k < 200; ++k, ++it)  // 200 steps: passes element 128, ++it moves to the next chunk
//     sum += *it;                      // through the freed index, sanitizer reports heap-use-after-free

std::vector<int> v{1, 2, 3, 4};
auto p = v.begin() + 2;                 // points at 3
v.erase(v.begin());
int x = *p;                             // 4: no crash, no report, wrong element
```

## Run: chunk.cpp

```bash
g++ -std=c++20 -O2 chunk.cpp -o chunk && ./chunk
```
`-O2` is the standard optimization level used across this repo.

```
sizeof(int) = 4
new chunk starts at element 128
new chunk starts at element 256
new chunk starts at element 384
new chunk starts at element 512
new chunk starts at element 640
new chunk starts at element 768
new chunk starts at element 896
```
Inside a chunk consecutive elements are 4 bytes apart. The address jumps every 128 elements, so a chunk holds 128 ints, 512 bytes, and the first chunk covers elements 0 to 127.

## Run: deque.cpp with the address sanitizer

```bash
g++ -std=c++20 -O1 -g -fsanitize=address deque.cpp -o deque_asan
./deque_asan 50
./deque_asan 200
```
`-fsanitize=address` adds run-time checks that report a read or write of freed memory, and `-g -O1` keep the stack traces readable with function names and line numbers. The argument is the number of steps taken through the stale iterator.

```
50 steps: sum 1039, expected 1039
```
Fifty steps stay inside the first chunk. The values are right, the sum matches the one read through a fresh iterator, and the sanitizer reports nothing.

```
==34609==ERROR: AddressSanitizer: heap-use-after-free on address 0x6fd8055e0040 at pc 0x642c3bf7056f bp 0x7fffef112940 sp 0x7fffef112938
READ of size 8 at 0x6fd8055e0040 thread T0
    #0 0x642c3bf7056e in std::_Deque_iterator<int, int&, int*>::_M_set_node(int**) <gcc>/bits/stl_deque.h:269
    #1 0x642c3bf7056e in std::_Deque_iterator<int, int&, int*>::operator++() <gcc>/bits/stl_deque.h:198
    #2 0x642c3bf7056e in main /home/val/learns/cpp/stl/iterator-invalidation/deque.cpp:37

0x6fd8055e0040 is located 32 bytes inside of 64-byte region [0x6fd8055e0020,0x6fd8055e0060)
freed by thread T0 here:
    #0 0x737806b2dfbf in operator delete(void*, unsigned long) (libasan.so.8+0x12dfbf)
    #4 0x642c3bf6feca in std::_Deque_base<int, std::allocator<int> >::_M_deallocate_map(int**, unsigned long) <gcc>/bits/stl_deque.h:607
    #5 0x642c3bf6feca in std::deque<int, std::allocator<int> >::_M_reallocate_map(unsigned long, bool) <gcc>/bits/deque.tcc:1146
    #6 0x642c3bf6feca in std::deque<int, std::allocator<int> >::_M_reserve_map_at_back(unsigned long) <gcc>/bits/stl_deque.h:2311
    #9 0x642c3bf6feca in main /home/val/learns/cpp/stl/iterator-invalidation/deque.cpp:29

previously allocated by thread T0 here:
    #4 0x642c3bf6f4c7 in std::_Deque_base<int, std::allocator<int> >::_M_allocate_map(unsigned long) <gcc>/bits/stl_deque.h:600
    #5 0x642c3bf6f4c7 in std::_Deque_base<int, std::allocator<int> >::_M_initialize_map(unsigned long) <gcc>/bits/stl_deque.h:648
    #8 0x642c3bf6f4c7 in main /home/val/learns/cpp/stl/iterator-invalidation/deque.cpp:26

SUMMARY: AddressSanitizer: heap-use-after-free /home/val/learns/cpp/stl/iterator-invalidation/deque.cpp:37 in main
```
The report is trimmed to the lines that matter. Frames that only forward a call are left out, and the path to the compiler's `include/c++/16.1.0` directory is shortened to `<gcc>`. The failing read is 8 bytes, one pointer, inside `_M_set_node`, called from `operator++` at `deque.cpp:37`, which is the `++it` of the loop over the stale iterator. It reads the address of the next chunk out of the chunk index. That index is a 64-byte array, eight chunk pointers, allocated by `_M_allocate_map` when the deque was constructed at line 26 and freed by `_M_deallocate_map` inside `_M_reallocate_map`, which a `push_back` at line 29 called when the index ran out of room. The first 127 increments stay in the first chunk and never touch the index. The 128th, the one after reading element 127, moves to the next chunk and does.

## Run: vector.cpp with the address sanitizer

```bash
g++ -std=c++20 -O1 -g -fsanitize=address vector.cpp -o vector_asan && ./vector_asan
```

```
x = 4
```
The iterator was taken at the 3. After the erase every later element moved down a slot, so the slot it points at holds 4. The slot is still inside the vector's memory, so there is no crash and nothing for the sanitizer to report. The program just reads the wrong element.

## Run: the checked mode

```bash
g++ -std=c++20 -O2 -D_GLIBCXX_DEBUG deque.cpp -o deque_dbg
./deque_dbg 50 2>&1 | head -12
./deque_dbg 200 2>&1 | head -12
g++ -std=c++20 -O2 -D_GLIBCXX_DEBUG vector.cpp -o vector_dbg && ./vector_dbg 2>&1 | head -12
```
`-D_GLIBCXX_DEBUG` turns on libstdc++'s debug mode, which swaps in checked versions of the containers and iterators that know which container they belong to and whether they are still valid. `2>&1 | head -12` sends the error message to the same stream as the output and keeps its first 12 lines.

```
<gcc>/debug/safe_iterator.h:352:
In function:
    constexpr gnu_debug::_Safe_iterator<_Iterator, _Sequence, 
    _Category>::reference gnu_debug::_Safe_iterator<_Iterator, _Sequence, 
    _Category>::operator*() const [with _Iterator = std::
    _Deque_iterator<int, int&, int*>; _Sequence = std::debug::deque<int>; 
    _Category = std::forward_iterator_tag; reference = int&]

Error: attempt to dereference a singular iterator.

Objects involved in the operation:
    iterator "this" @ 0x7ffcc768e0e0 {
```
The 50-step and 200-step runs print the same error, and neither prints a sum, so the program stopped on the first dereference of the stale iterator, before reading a value. The vector run stops the same way:

```
<gcc>/debug/safe_iterator.h:352:
In function:
    constexpr gnu_debug::_Safe_iterator<_Iterator, _Sequence, 
    _Category>::reference gnu_debug::_Safe_iterator<_Iterator, _Sequence, 
    _Category>::operator*() const [with _Iterator = gnu_cxx::
    normal_iterator<int*, std::vector<int, std::allocator<int> > >; 
    _Sequence = std::debug::vector<int>; _Category = 
    std::forward_iterator_tag; reference = int&]

Error: attempt to dereference a singular iterator.

Objects involved in the operation:
```
The checked mode tracks validity by container, not by memory, so it reports the quiet vector case that the sanitizer cannot see, and it reports the deque case at the first use regardless of how far the loop would have gone.

## Run: assertions mode

```bash
g++ -std=c++20 -O2 -D_GLIBCXX_ASSERTIONS deque.cpp -o deque_assert && ./deque_assert 50
g++ -std=c++20 -O2 -D_GLIBCXX_ASSERTIONS vector.cpp -o vector_assert && ./vector_assert
```
`-D_GLIBCXX_ASSERTIONS` is a lighter option that adds cheap checks such as bounds checking on `operator[]`.

```
50 steps: sum 1039, expected 1039
x = 4
```
Neither case is caught. The deque run passes and the vector run reads the wrong element, same as with no flag. Detecting a stale iterator needs `-D_GLIBCXX_DEBUG`.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 16.1.0
