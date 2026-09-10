#include <iostream>
#include <utility>

/*
No inheritance, no virtual keyword anywhere in this file. The template
constructor is what makes unrelated types work behind one interface: it
instantiates a distinct pair of function pointers for whichever type T
it's built from, a lambda that knows exactly how to cast data back to T*
and call the right operation. Shape's own type never changes, only the
function pointers stored inside a given instance do.
*/
class Shape {
    void* data;
    void (*draw_fn)(const void*);
    void (*destroy_fn)(void*);

public:
    template<typename T>
    Shape(T obj)
        : data(new T(std::move(obj)))
        , draw_fn([](const void* p) { static_cast<const T*>(p)->draw(); })
        , destroy_fn([](void* p) { delete static_cast<T*>(p); })
    {}

    void draw() const { draw_fn(data); }
    ~Shape() { destroy_fn(data); }
};

// two unrelated types, no shared base, no virtual functions anywhere
struct Circle { void draw() const { std::cout << "Circle::draw\n"; } };
struct Square { void draw() const { std::cout << "Square::draw\n"; } };

int main() {
    Shape shapes[] = { Circle{}, Square{} };
    for (auto& s : shapes) s.draw(); // dispatches correctly, no vtable involved
}
