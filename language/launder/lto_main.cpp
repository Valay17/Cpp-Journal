#include <iostream>
#include <new>

/*
The strongest single attempt made in this repo entry to get omitting
std::launder to visibly misbehave: separate translation units, heap
allocation, a const member, and link-time optimization, so the compiler
has cross-unit visibility into make/destroy/read_it and every
opportunity to apply the most aggressive analysis it has available.
Still confirmed to produce a=1 b=2 c=2 on both GCC 13.3.0 and 16.1.0, at
-O2 and -O3, with -flto.
*/

struct X { const int n; };
X* make(void* buf, int v);
void destroy(X* p);
int read_it(X* p);

int main() {
    void* buf = operator new(sizeof(X));
    X* p = make(buf, 1);

    int a = read_it(p);

    destroy(p);
    make(buf, 2);

    int b = read_it(p);                 // no launder
    int c = read_it(std::launder(p));   // laundered

    std::cout << "a=" << a << " b=" << b << " c=" << c << "\n";
    operator delete(buf);
}
