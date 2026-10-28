#include <cstdio>
#include <cstdlib>
#include <cstddef>

/*
PoolAllocator reserves one region up front and cuts it into equal chunks.
A free chunk stores the pointer to the next free chunk inside itself, so
the free list costs no extra memory, and a chunk that is in use carries no
bookkeeping at all. allocate pops the first chunk off the list and
deallocate pushes one back, neither searches.

chunk_size must be at least sizeof(void*) and a multiple of the alignment
the objects need. malloc returns a region aligned for any object, and
every chunk stays aligned as long as chunk_size is a multiple of that
alignment.
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

int main() {
    PoolAllocator pool(1024, 4);
    void* a = pool.allocate(); void* b = pool.allocate();
    void* c = pool.allocate(); void* d = pool.allocate();
    printf("fifth is null: %d, distinct: %d\n",
           pool.allocate() == nullptr, a != b && b != c && c != d);
    pool.deallocate(b);
    printf("freed chunk reused: %d\n", pool.allocate() == b);
}
