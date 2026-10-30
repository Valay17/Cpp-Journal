#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>

/*
futex(FUTEX_WAIT, addr, expected) puts the caller to sleep only if the
number at addr still equals expected when the kernel checks. If it has
changed, the call returns at once with EAGAIN and the thread never sleeps.
That check is what lets a thread announce "someone is waiting" by setting
the number to 2 and then sleep without missing a wake-up: if the owner
unlocked in between, the number is no longer 2 and the wait refuses.

Case 1: the number is 1, the caller expects 2, so the wait returns EAGAIN
immediately. Case 2: the number is 2, the caller expects 2, so it would
sleep, and a 100 ms timeout makes it give up with ETIMEDOUT afterward.
*/

static int word;

static long futex_wait(int expected, const timespec* timeout) {
    return syscall(SYS_futex, &word, FUTEX_WAIT_PRIVATE, expected, timeout, nullptr, 0);
}

int main() {
    word = 1;
    long r = futex_wait(2, nullptr);
    printf("number is 1, expect 2: returned %ld, errno %s\n", r, r < 0 ? strerror(errno) : "none");

    word = 2;
    timespec ts{0, 100'000'000};
    timespec before, after;
    clock_gettime(CLOCK_MONOTONIC, &before);
    r = futex_wait(2, &ts);
    clock_gettime(CLOCK_MONOTONIC, &after);
    long ms = (after.tv_sec - before.tv_sec) * 1000 + (after.tv_nsec - before.tv_nsec) / 1000000;
    printf("number is 2, expect 2: returned %ld, errno %s, slept about %ld ms\n",
           r, r < 0 ? strerror(errno) : "none", ms);
}
