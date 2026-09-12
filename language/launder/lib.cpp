#include <new>

/*
Compiled as a separate translation unit specifically so the LTO build in
the README's second run represents cross-unit optimization, not
inlining GCC could already do within a single file.
*/

struct X { const int n; };

X* make(void* buf, int v) {
    return new (buf) X{v};
}

void destroy(X* p) {
    p->~X();
}

int read_it(X* p) {
    return p->n;
}
