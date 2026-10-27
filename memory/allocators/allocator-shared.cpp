#include <memory>
#include <iostream>
#include <cstdlib>
#include <new>

/*
The global operator new is replaced so every heap allocation is counted and
its size recorded. allocate_shared asks its allocator for one block that
holds both the shared_ptr bookkeeping (the reference counts) and the int,
and builds the int in a slice of it. shared_ptr<int>(new int) allocates the
int on its own, and the shared_ptr then allocates a separate control block.
*/

static int g_allocs = 0;
static std::size_t g_last_size = 0;

void* operator new(std::size_t n) {
    ++g_allocs;
    g_last_size = n;
    void* p = std::malloc(n);
    if (!p) throw std::bad_alloc();
    return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

int main() {
    g_allocs = 0;
    {
        auto s = std::allocate_shared<int>(std::allocator<int>{}, 7);
        std::cout << "allocate_shared<int>:     allocations=" << g_allocs
                  << " block=" << g_last_size << " bytes\n";
    }

    g_allocs = 0;
    {
        std::shared_ptr<int> s(new int(7));
        std::cout << "shared_ptr<int>(new int): allocations=" << g_allocs << "\n";
    }
}
