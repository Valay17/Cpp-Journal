#include <iostream>
#include <cstdlib>
#include <new>

/*
Same type, four different ways to get an instance of it into memory.
Widget prints from its own constructor and destructor, so which of these
mechanisms actually built or destroyed an object is visible directly in
the program's output, not something that has to be inferred.
*/
struct Widget {
    Widget() { std::cout << "  Widget constructed\n"; }
    ~Widget() { std::cout << "  Widget destructed\n"; }
};

int main() {
    std::cout << "=== new / delete: allocates and constructs, destructs and deallocates ===\n";
    Widget* a = new Widget();
    delete a;

    std::cout << "=== operator new / operator delete: allocation only, no constructor or destructor ===\n";
    void* raw = operator new(sizeof(Widget));
    std::cout << "  (no constructor ran)\n";
    operator delete(raw);
    std::cout << "  (no destructor ran)\n";

    std::cout << "=== placement new: construction only, no allocation ===\n";
    alignas(Widget) char buffer[sizeof(Widget)];
    Widget* p = new (buffer) Widget();
    std::cout << "  (no allocation happened, buffer already existed)\n";
    p->~Widget(); // no delete, nothing was allocated to release

    std::cout << "=== malloc / free: raw bytes only, never touches a constructor or destructor ===\n";
    Widget* m = static_cast<Widget*>(malloc(sizeof(Widget)));
    std::cout << "  (no constructor ran)\n";
    free(m);
    std::cout << "  (no destructor ran)\n";
}
