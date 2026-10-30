# A Mutex Is a Number That Is 0, 1 or 2

Full writeup: https://valay17.github.io/Portfolio/blog/concurrency/mutex-internals

A mutex on Linux is a number that is 0, 1 or 2. The difference between 1 and 2 decides whether unlocking has to call the kernel.

`std::mutex` is a thin wrapper over glibc's `pthread_mutex_t`, and its lock state is one 4-byte number. 0 means free. 1 means locked with nobody waiting. 2 means locked with at least one thread waiting.

Locking is one atomic instruction that tries to change 0 to 1. If it works, you own the lock and the kernel never hears about it. If it fails, the thread sets the number to 2, announcing that someone is waiting, and asks the kernel to put it to sleep with `futex`. The kernel only does that if the number is still 2 at that moment. If the lock was released in between, the thread doesn't sleep, so it can't miss its wake-up.

Unlocking sets the number to 0 and checks what it was. If it was 1, nobody was waiting and the kernel is never involved. If it was 2, the thread calls `futex` to wake one sleeper. The mutex doesn't keep a list of waiting threads. It keeps one fact, whether anyone is waiting, and lets the kernel remember the rest.

## What the code demonstrates

Three files, one per claim.

`state.cpp` prints the lock word at each step of a lock that gets contended, first on a `pthread_mutex_t` and then on a `std::mutex` through `native_handle()`, which returns the `pthread_mutex_t` it wraps. A second thread finds the lock held, sets the number to 2 and sleeps, and the main thread reads 2 before unlocking. The sleeper also records the number right after it gets the lock. Run under `strace`, the `futex` calls around the lock word can be matched to its printed address.

`uncontended.cpp` locks and unlocks a `std::mutex` 1,000,000 times from one thread. Every lock finds 0 and every unlock finds 1, so the kernel is never involved.

`futex_value.cpp` calls the `futex` system call directly to show the check that stops a thread from missing its wake-up: a wait that expects 2 on a number that is not 2 returns at once, and a wait that expects 2 on a number that is 2 sleeps.

## Key insight

```cpp
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
printf("%d\n", m.__data.__lock);                     // 0: free

pthread_mutex_lock(&m);
printf("%d\n", m.__data.__lock);                     // 1: locked, nobody waiting

std::thread t([&] { pthread_mutex_lock(&m); pthread_mutex_unlock(&m); });   // finds it held, sets 2, sleeps in futex
sleep(1);                                            // give it time to block

printf("%d\n", m.__data.__lock);                     // 2: locked, one thread waiting

pthread_mutex_unlock(&m);                            // sees 2, wakes the sleeper
t.join();
printf("%d\n", m.__data.__lock);                     // 0: free again
```

## Run: state.cpp

```bash
g++ -std=c++20 -O2 -pthread state.cpp -o state
./state
```
`-O2` is the standard optimization level used across this repo, and `-pthread` is required for `std::thread` with GCC on Linux. `sleep(1)` gives the second thread time to reach `futex` before the main thread reads the number, so the 2 depends on that timing and not on any ordering guarantee.

```
sizeof(pthread_mutex_t) = 40, sizeof(__data.__lock) = 4
lock word at 0x7ffeda056250

pthread_mutex_t:
0
1
2
0
2  (value the woken thread saw while it held the lock)

std::mutex:      sizeof = 40, same size as pthread_mutex_t: 1
0
1
2
0
```
The lock state is a 4-byte number inside a 40-byte `pthread_mutex_t`. It goes 0, 1, 2, 0 as the snippet describes, and `std::mutex` is the same size and goes through the same four values, which is what a thin wrapper looks like. The last line of the first block is the one the post doesn't mention: the thread that slept does not get the lock back with the number at 1. It holds the lock with the number at 2, because it cannot know whether other threads are still waiting.

```bash
strace -f -e trace=futex ./state
```
`strace` prints the system calls a program makes. `-f` follows the threads the program creates and `-e trace=futex` limits it to `futex`. The program's own output is interleaved with `strace`'s in the terminal and is left out of the lines below.

```
futex(0x6ffc508b96bc, FUTEX_WAKE_PRIVATE, 2147483647) = 0
[pid 17067] futex(0x7ffd98f46550, FUTEX_WAIT_PRIVATE, 2, NULL <unfinished ...>
[pid 17066] futex(0x7ffd98f46550, FUTEX_WAKE_PRIVATE, 1) = 1
[pid 17067] <... futex resumed>)        = 0
[pid 17066] futex(0x6ffc501ff990, FUTEX_WAIT_BITSET|FUTEX_CLOCK_REALTIME, 17067, NULL, FUTEX_BITSET_MATCH_ANY <unfinished ...>
[pid 17067] futex(0x7ffd98f46550, FUTEX_WAKE_PRIVATE, 1) = 0
[pid 17066] <... futex resumed>)        = 0
```
The address in the `FUTEX_WAIT_PRIVATE` call, 0x7ffd98f46550, is the lock word the program printed in that run. The sleeper asks the kernel to put it to sleep only if the number is still 2, and the main thread's unlock calls `FUTEX_WAKE_PRIVATE` for one waiter, which returns 1 for the one thread it woke. The woken thread's own unlock then calls `FUTEX_WAKE_PRIVATE` as well and it returns 0, nobody was left to wake. That matches the 2 it saw while holding the lock. The same sequence repeats for the `std::mutex`, at its own address. The `FUTEX_WAIT_BITSET` on a different address is the main thread waiting in `join`, and the first line is libc starting up.

## Run: uncontended.cpp

```bash
g++ -std=c++20 -O2 -pthread uncontended.cpp -o uncontended
./uncontended
strace -f -e trace=futex ./uncontended
```

```
locked and unlocked 1000000 times
futex(0x7240d2cb96bc, FUTEX_WAKE_PRIVATE, 2147483647) = 0
locked and unlocked 1000000 times
+++ exited with 0 +++
```
One thread, a million lock and unlock pairs, and `strace` shows a single `futex` call. It is the same libc startup wake as above, before the loop starts. The loop itself never reaches the kernel.

## Run: futex_value.cpp

```bash
g++ -std=c++20 -O2 futex_value.cpp -o futex_value
./futex_value
```

```
number is 1, expect 2: returned -1, errno Resource temporarily unavailable
number is 2, expect 2: returned -1, errno Connection timed out, slept about 100 ms
```
When the number is 1 and the caller expects 2, the wait returns at once with `EAGAIN` (shown as "Resource temporarily unavailable") and the thread never sleeps. That is the case where the owner unlocked between the thread setting 2 and asking the kernel to sleep. When the number is 2, the wait sleeps, and here it gives up after the 100 ms timeout with `ETIMEDOUT`.

## Scope

`__data.__lock` is a glibc field, so `state.cpp` only builds against glibc, and the 0, 1, 2 meaning is that of glibc's normal mutex on Linux. Other C libraries, recursive and error-checking mutexes, and `std::shared_mutex` were not tested.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 16.1.0
