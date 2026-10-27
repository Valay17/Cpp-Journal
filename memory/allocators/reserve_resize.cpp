#include <vector>
#include <iostream>
#include <new>
#include <utility>

/*
LoggingAllocator is a minimal allocator that reports its two jobs separately.
allocate hands back raw bytes with no object alive in them. construct builds
one object in one slot of those bytes. std::vector calls them through
std::allocator_traits, so every allocation and every constructed element
shows up here.

reserve only needs the bytes, so it calls allocate and nothing else. resize
needs elements to exist, so it calls construct once per new element.
*/

template <class T>
struct LoggingAllocator {
    using value_type = T;

    LoggingAllocator() = default;
    template <class U> LoggingAllocator(const LoggingAllocator<U>&) {}

    T* allocate(std::size_t n) {
        std::cout << "  allocate(" << n << ")\n";
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate(T* p, std::size_t) { ::operator delete(p); }

    template <class U, class... Args>
    void construct(U* p, Args&&... args) {
        ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
        ++constructs;
    }

    static inline int constructs = 0;
};

template <class T, class U>
bool operator==(const LoggingAllocator<T>&, const LoggingAllocator<U>&) { return true; }

int main() {
    std::vector<int, LoggingAllocator<int>> v;

    std::cout << "reserve(100):\n";
    v.reserve(100);
    std::cout << "  size=" << v.size() << " capacity=" << v.capacity()
              << " constructs=" << LoggingAllocator<int>::constructs << "\n";

    std::cout << "resize(100):\n";
    v.resize(100);
    std::cout << "  size=" << v.size() << " capacity=" << v.capacity()
              << " constructs=" << LoggingAllocator<int>::constructs << "\n";
}
