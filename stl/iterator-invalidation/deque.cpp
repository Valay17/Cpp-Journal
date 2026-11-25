#include <cstdio>
#include <cstdlib>
#include <deque>

/*
A deque stores its elements in fixed-size chunks and keeps the addresses of
the chunks in an array called the index. push_back at the end can run out of
room in that index, so it allocates a bigger one, copies the chunk addresses
over and frees the old one.

it is taken before the 100,000 push_backs, so after them it is stale: it
still points at the first element inside a chunk that exists, but its link to
the index is the old, freed array. Stepping inside the first chunk does not
touch the index, so the values read are right. Stepping past the end of the
chunk makes ++it read the next chunk's address out of the old index, which
is freed memory.

The number of steps comes from the command line, and the program prints the
sum of the values it read next to the sum read through a fresh iterator, so
a wrong value would show up.
*/

int main(int argc, char** argv) {
    int steps = argc > 1 ? atoi(argv[1]) : 50;

    std::deque<int> d(4, 1);
    auto it = d.begin();                    // iterator to the first element
    for (int i = 0; i < 100000; ++i)
        d.push_back(i);                     // chunk index reallocates, the old one is freed: it is now stale

    long expected = 0;
    auto fresh = d.begin();
    for (int k = 0; k < steps; ++k, ++fresh)
        expected += *fresh;

    long sum = 0;
    for (int k = 0; k < steps; ++k, ++it)
        sum += *it;

    printf("%d steps: sum %ld, expected %ld\n", steps, sum, expected);
}
