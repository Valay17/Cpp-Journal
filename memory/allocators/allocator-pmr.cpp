#include <vector>
#include <memory_resource>
#include <iostream>
#include <cstdlib>
#include <new>

/*
The global operator new is replaced so every heap allocation is counted.
The same loop runs twice: once on a plain std::vector, which takes its
bytes from the heap each time it grows, and once on a std::pmr::vector
whose memory resource is a buffer on the stack. The container and the loop
are identical. Only the source of the bytes changes.
*/

static int g_allocs = 0;

void* operator new(std::size_t n) {
    ++g_allocs;
    void* p = std::malloc(n);
    if (!p) throw std::bad_alloc();
    return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

int main() {
    g_allocs = 0;
    {
        std::vector<int> v;
        for (int i = 0; i < 100; ++i)
            v.push_back(i);
        std::cout << "std::vector:      heap allocations=" << g_allocs << "\n";
    }

    g_allocs = 0;
    {
        char buf[4096];
        std::pmr::monotonic_buffer_resource res(buf, sizeof buf);
        std::pmr::vector<int> p(&res);
        for (int i = 0; i < 100; ++i)
            p.push_back(i);
        std::cout << "std::pmr::vector: heap allocations=" << g_allocs << "\n";
    }
}
