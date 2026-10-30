#include <pthread.h>
#include <unistd.h>
#include <cstdio>
#include <mutex>
#include <thread>

/*
glibc's pthread_mutex_t keeps its lock state in one 4-byte int, __data.__lock.
0 means free, 1 means locked with nobody waiting, 2 means locked with at
least one thread waiting. This file prints that int at each step of a lock
that gets contended, using the pthread_mutex_t directly and then using a
std::mutex, whose native_handle() returns the pthread_mutex_t it wraps.

The second thread finds the mutex held, so it sets the number to 2 and
sleeps in futex. sleep(1) gives it time to get there before the main thread
reads the number. The address of the lock word is printed once so it can be
matched against the futex calls strace reports.

The sleeper also records the number right after it gets the lock. It is not
back to 1: a thread that had to sleep takes the lock by setting 2 again,
because it cannot know whether other threads are still waiting.
*/

int main() {
    pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
    int seen_by_woken_thread = -1;
    printf("sizeof(pthread_mutex_t) = %zu, sizeof(__data.__lock) = %zu\n",
           sizeof(m), sizeof(m.__data.__lock));
    printf("lock word at %p\n\n", (void*)&m.__data.__lock);

    printf("pthread_mutex_t:\n");
    printf("%d\n", m.__data.__lock);                     // 0: free

    pthread_mutex_lock(&m);
    printf("%d\n", m.__data.__lock);                     // 1: locked, nobody waiting

    std::thread t([&] { pthread_mutex_lock(&m); seen_by_woken_thread = m.__data.__lock; pthread_mutex_unlock(&m); });
    sleep(1);                                            // give it time to block

    printf("%d\n", m.__data.__lock);                     // 2: locked, one thread waiting

    pthread_mutex_unlock(&m);                            // sees 2, wakes the sleeper
    t.join();
    printf("%d\n", m.__data.__lock);                    // 0: free again
    printf("%d  (value the woken thread saw while it held the lock)\n\n", seen_by_woken_thread);

    std::mutex sm;
    pthread_mutex_t* h = sm.native_handle();
    printf("std::mutex:      sizeof = %zu, same size as pthread_mutex_t: %d\n",
           sizeof(sm), sizeof(sm) == sizeof(pthread_mutex_t));
    printf("%d\n", h->__data.__lock);

    sm.lock();
    printf("%d\n", h->__data.__lock);

    std::thread t2([&] { sm.lock(); sm.unlock(); });
    sleep(1);

    printf("%d\n", h->__data.__lock);

    sm.unlock();
    t2.join();
    printf("%d\n", h->__data.__lock);
}
