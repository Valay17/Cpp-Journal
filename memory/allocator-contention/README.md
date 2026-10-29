# A Lock-Free Queue That Still Waits on a Lock

Full writeup: https://valay17.github.io/Portfolio/blog/memory/allocator-contention

A lock-free queue can still end up waiting on a lock. The lock is inside `malloc`.

Lock-free means no thread has to wait for another one to make progress. A queue built on atomics can promise that. The allocator underneath it usually can't. Take a queue with one producer and one consumer: the producer calls `malloc` to build a node and pushes it, the consumer pops it and calls `free`. Nothing in the queue ever blocks, but every node is allocated on one thread and freed on the other.

glibc's `malloc` keeps memory in regions called arenas, and each arena has a lock. A `free` from another thread has to take the lock of the arena the block came from, except for small blocks, which go into fast lists (the fastbins) updated with atomic operations. Anything bigger takes the lock, and the producer's next `malloc` needs the same one. The system call a thread makes to wait on a lock is `futex`, so counting `futex` calls shows where the waiting happens.

The fix is to take allocation off the hot path. Allocate every node that can be in flight at startup, and have the consumer hand nodes back through a second queue instead of calling `free`.

## What the code demonstrates

One file, two ways of moving 1,000,000 nodes from a producer thread to a consumer thread through a lock-free single-producer single-consumer queue.

In `malloc` mode the producer builds each node with `new` and the consumer calls `delete`. In `pool` mode all the nodes the queue can hold are allocated before the threads start, the consumer returns each node through a second queue, and the producer takes its next node from there, so neither thread calls the allocator after startup. The node size is a command-line argument. The consumer adds up the values it receives and the program checks the sum, and it prints the number of voluntary context switches, which counts the times a thread slept and gave up the CPU.

## Key insight

```cpp
// producer thread
queue.push(new Node(val));        // queue is lock-free, but this new calls malloc

// consumer thread
Node* n = queue.pop();
delete n;                         // free from another thread: bigger blocks take the arena's lock

// fix: nodes are allocated once at startup and recycled through a second queue

// producer thread
Node* n = free_nodes.pop();       // no allocator call
queue.push(new (n) Node(val));    // construct in place

// consumer thread
Node* n = queue.pop();
n->~Node();
free_nodes.push(n);               // hand the node back, no free
```

The price is sizing the pool for the most nodes that can be in flight. Here that is the queue's capacity plus one node held by each thread. If the pool runs dry there is no lock-free fallback, so running out is a sizing bug and not something to handle at runtime.

## Run: futex.cpp

```bash
g++ -std=c++20 -O2 -pthread futex.cpp -o futex
./futex malloc 64
./futex malloc 512
./futex pool 512
```
`-O2` is the standard optimization level used across this repo, and `-pthread` is required for `std::thread` with GCC on Linux. The first argument picks the mode and the second is the node size in bytes.

```
malloc, 64-byte nodes: sum ok, voluntary context switches 11
malloc, 512-byte nodes: sum ok, voluntary context switches 4567
pool, 512-byte nodes: sum ok, voluntary context switches 1
```
Every run delivered all 1,000,000 nodes. With 64-byte nodes the threads almost never sleep. With 512-byte nodes they sleep thousands of times, and with the pool they barely sleep at all.

## Run: counting futex calls

```bash
strace -f -c -e trace=futex ./futex malloc 64
strace -f -c -e trace=futex ./futex malloc 128
strace -f -c -e trace=futex ./futex malloc 512
strace -f -c -e trace=futex ./futex pool 512
```
`strace` prints the system calls a program makes. `-f` follows the threads the program creates, `-c` prints a summary count at the end instead of every call, and `-e trace=futex` limits it to `futex`. Each command was run three times.

```
malloc 64:  2 calls    2 calls    2 calls
malloc 128: 26497 calls    25423 calls    24107 calls
malloc 512: 103035 calls   99850 calls    107934 calls
pool 512:   3 calls    2 calls    2 calls
```
The two calls at 64 bytes are one wake during program startup and the main thread waiting in `join` for a worker thread to finish. At 128 bytes and above the allocator's lock is contended and the count jumps by four orders of magnitude. With the pool it is back to the same two or three calls.

## Run: switching the fast lists off

```bash
GLIBC_TUNABLES=glibc.malloc.mxfast=0 strace -f -c -e trace=futex ./futex malloc 64
GLIBC_TUNABLES=glibc.malloc.tcache_count=0 strace -f -c -e trace=futex ./futex malloc 64
GLIBC_TUNABLES=glibc.malloc.mxfast=0:glibc.malloc.tcache_count=0 strace -f -c -e trace=futex ./futex malloc 64
GLIBC_TUNABLES=glibc.malloc.mxfast=0:glibc.malloc.tcache_count=0 strace -f -c -e trace=futex ./futex pool 64
```
`GLIBC_TUNABLES` changes glibc's allocator settings for one run. `glibc.malloc.mxfast=0` turns the fastbins off, and `glibc.malloc.tcache_count=0` turns off the per-thread cache of recently freed blocks.

```
glibc.malloc.mxfast=0:                                  20742 calls   17759 calls   23928 calls
glibc.malloc.tcache_count=0:                            2 calls       2 calls       3 calls
glibc.malloc.mxfast=0:glibc.malloc.tcache_count=0:      20049 calls   30706 calls   26188 calls
both off, pool 64:                                      5 calls
```
With the fastbins off, the 64-byte nodes wait just like the larger ones. Turning off only the per-thread cache changes nothing, so the fastbins are what keep the small nodes from taking the lock. With both off, the pool run still makes only 5 calls, because it never calls the allocator after startup.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 16.1.0
