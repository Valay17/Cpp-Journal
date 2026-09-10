#include <iostream>

/*
Classic CRTP: the base class template calls back into the derived type
through static_cast, resolved entirely at compile time since the compiler
already knows the concrete type at the point Shape<Circle> is instantiated.
*/
template<typename Derived>
struct Shape {
    double area() const {
        return static_cast<const Derived*>(this)->area_impl();
    }
};

struct Circle : Shape<Circle> {
    double r;
    double area_impl() const { return 3.14159 * r * r; }
};

/*
C++23 deducing this: the same shape, no template base class, no manual
static_cast. The object itself is an explicit, deducible parameter, and
the derived type gets deduced automatically at the call site.
*/
struct ShapeNew {
    auto area(this auto&& self) {
        return self.area_impl();
    }
};

struct CircleNew : ShapeNew {
    double r;
    double area_impl() const { return 3.14159 * r * r; }
};

// free functions so each has a clean, individually disassemblable symbol
double compute_classic(const Circle& c) { return c.area(); }
double compute_new(const CircleNew& c) { return c.area(); }

int main() {
    Circle c1{{}, 7.0};
    CircleNew c2{{}, 7.0};

    std::cout << "classic: " << compute_classic(c1) << "\n";
    std::cout << "new:     " << compute_new(c2) << "\n";
}
