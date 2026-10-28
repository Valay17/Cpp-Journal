#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>

/*
First wave: 100,000 connections, each a 1 KB scratch buffer then a 64-byte
session object, then every scratch buffer is freed. That leaves 100,000
1 KB holes when the scratch buffers came from malloc, or 100,000 free
chunks when they came from a pool.

Between the two waves, other code optionally allocates 100,000 small
200-byte objects with malloc. Second wave: 100,000 more 1 KB scratch
buffers from the same allocator as the first.

Run it three ways. "malloc 0" has nothing in between. "malloc 1" lets the
small objects take pieces out of the holes. "pool 1" gives the scratch
buffers their own pool, so the small objects have no holes to take from.
rss_kb reads resident memory from /proc/self/statm.
*/

class PoolAllocator {
    struct Node { Node* next; };
    char* region = nullptr;
    Node* free_list = nullptr;

public:
    PoolAllocator(size_t chunk_size, size_t count)
        : region(static_cast<char*>(std::malloc(chunk_size * count))) {
        for (size_t i = 0; i < count; ++i)
            deallocate(region + i * chunk_size);
    }

    ~PoolAllocator() { std::free(region); }
    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    void* allocate() {
        if (!free_list) return nullptr;
        Node* p = free_list;
        free_list = free_list->next;
        return p;
    }

    void deallocate(void* p) {
        Node* n = static_cast<Node*>(p);
        n->next = free_list;
        free_list = n;
    }
};


static long rss_kb() {
    long pages = 0, x;
    FILE* f = fopen("/proc/self/statm", "r");
    if (f) { if (fscanf(f, "%ld %ld", &x, &pages) != 2) pages = 0; fclose(f); }
    return pages * sysconf(_SC_PAGESIZE) / 1024;
}

int main(int argc, char** argv) {
    const size_t N = 100000;
    bool use_pool = argc > 1 && std::string(argv[1]) == "pool";
    bool smalls = argc > 2 && atoi(argv[2]) == 1;

    PoolAllocator* pool = use_pool ? new PoolAllocator(1024, N) : nullptr;
    auto get_scratch = [&]() { return use_pool ? pool->allocate() : malloc(1024); };
    auto put_scratch = [&](void* p) { if (use_pool) pool->deallocate(p); else free(p); };

    std::vector<void*> scratch(N), session(N), small(N), wave2(N);
    for (size_t i = 0; i < N; ++i) {
        scratch[i] = get_scratch(); memset(scratch[i], 1, 1024);
        session[i] = malloc(64);    memset(session[i], 1, 64);
    }
    for (size_t i = 0; i < N; ++i) put_scratch(scratch[i]);
    long a = rss_kb();

    if (smalls)
        for (size_t i = 0; i < N; ++i) { small[i] = malloc(200); memset(small[i], 3, 200); }
    long b = rss_kb();

    for (size_t i = 0; i < N; ++i) { wave2[i] = get_scratch(); memset(wave2[i], 2, 1024); }
    long c = rss_kb();

    printf("%-6s smalls=%d | after freeing scratch %ld KB | after smalls %ld KB (+%ld) | after 2nd wave of 1 KB %ld KB (+%ld KB = %ld MB)\n",
           use_pool ? "pool" : "malloc", smalls, a, b, b - a, c, c - b, (c - b) / 1024);
}
