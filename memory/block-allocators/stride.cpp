#include <cstdio>
#include <cstdlib>
#include <malloc.h>

/*
Two consecutive 16-byte requests: how far apart do the blocks land, and how
many bytes does malloc say are usable in the first one? The gap between
the two is the space each block occupies, and anything beyond the usable
size is bookkeeping malloc keeps for itself.
*/

int main() {
    void* a = malloc(16);
    void* b = malloc(16);
    printf("malloc(16): addresses differ by %ld bytes, usable size %zu\n",
           (long)((char*)b - (char*)a), malloc_usable_size(a));
    free(a); free(b);
}
