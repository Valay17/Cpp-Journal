#include <mutex>
#include <cstdio>

/*
One thread locks and unlocks a std::mutex 1,000,000 times. Nobody else ever
touches it, so every lock finds the number at 0 and changes it to 1, and
every unlock finds it at 1. The kernel is never involved, which strace
confirms by showing no futex calls from the loop.
*/

int main() {
    std::mutex m;
    long count = 0;
    for (int i = 0; i < 1'000'000; ++i) {
        m.lock();
        ++count;
        m.unlock();
    }
    printf("locked and unlocked %ld times\n", count);
}
