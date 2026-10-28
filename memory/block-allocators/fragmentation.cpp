#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <unistd.h>

/*
Each of 100,000 connections gets a 1 KB scratch buffer and then a 64-byte
session object, allocated back to back, so memory alternates scratch,
session, scratch, session. Once setup finishes, every scratch buffer is
freed, which leaves 100,000 separate 1 KB holes, each next to a live
session object.

The next stage then allocates 100,000 buffers of the size given on the
command line. rss_kb reads the process's resident memory from
/proc/self/statm, so growth shows up as memory the process took from
the OS. A buffer that fits a hole reuses it. A buffer that does not fit
any hole needs fresh memory.
*/

static long rss_kb() {
    long pages = 0, x;
    FILE* f = fopen("/proc/self/statm", "r");
    if (f) { if (fscanf(f, "%ld %ld", &x, &pages) != 2) pages = 0; fclose(f); }
    return pages * sysconf(_SC_PAGESIZE) / 1024;
}

int main(int argc, char** argv) {
    const size_t N = 100000;
    const size_t SESSION = 64, SCRATCH = 1024;
    size_t req = argc > 1 ? (size_t)atoi(argv[1]) : SCRATCH;
    std::vector<void*> session(N), scratch(N), next(N);

    for (size_t i = 0; i < N; ++i) {
        scratch[i] = malloc(SCRATCH); memset(scratch[i], 1, SCRATCH);
        session[i] = malloc(SESSION); memset(session[i], 1, SESSION);
    }
    long after_setup = rss_kb();

    for (size_t i = 0; i < N; ++i) free(scratch[i]);
    long after_free = rss_kb();

    for (size_t i = 0; i < N; ++i) { next[i] = malloc(req); memset(next[i], 2, req); }
    long after_next = rss_kb();

    printf("next buffers of %4zu B: RSS after setup %ld KB, after freeing scratch %ld KB, after %zu new buffers %ld KB (grew %ld KB, %ld MB)\n",
           req, after_setup, after_free, N, after_next, after_next - after_free, (after_next - after_free) / 1024);
}
