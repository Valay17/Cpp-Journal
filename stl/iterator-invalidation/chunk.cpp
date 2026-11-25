#include <cstdio>
#include <deque>

/*
Finds where a deque<int> changes chunks. Inside a chunk, consecutive elements
are sizeof(int) bytes apart. At a chunk boundary the next element is in a
different allocation, so the gap is something else. The program prints the
element index and the gap at every boundary it finds in the first 1000
elements, which gives the number of ints per chunk.
*/

int main() {
    std::deque<int> d(4, 1);
    for (int i = 0; i < 1000; ++i)
        d.push_back(i);

    printf("sizeof(int) = %zu\n", sizeof(int));
    for (std::size_t i = 1; i < 1000; ++i) {
        const char* prev = reinterpret_cast<const char*>(&d[i - 1]);
        const char* cur  = reinterpret_cast<const char*>(&d[i]);
        if (cur - prev != static_cast<long>(sizeof(int)))
            printf("new chunk starts at element %zu\n", i);
    }
}
