#include <iostream>
#include <cstdlib>

/*
operator new is an ordinary function, not a language keyword, which is why
it can be overloaded per class. Ordinary new/delete syntax at the call
site never changes, the redirection happens entirely inside the class.
This is the mechanism every custom allocator or memory pool actually
hooks into.
*/
struct PooledWidget {
    PooledWidget() { std::cout << "  PooledWidget constructed\n"; }
    ~PooledWidget() { std::cout << "  PooledWidget destructed\n"; }

    static void* operator new(size_t size) {
        std::cout << "  custom operator new called, size=" << size << "\n";
        return malloc(size);
    }
    static void operator delete(void* p) {
        std::cout << "  custom operator delete called\n";
        free(p);
    }
};

int main() {
    std::cout << "=== plain 'new PooledWidget()', calling code unchanged ===\n";
    PooledWidget* w = new PooledWidget(); // ordinary new syntax
    delete w;                             // ordinary delete syntax
}
