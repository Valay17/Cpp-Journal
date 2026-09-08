#include <iostream>
#include <utility>

/*
Same variable, same declared type, three different constructions, decided
entirely by the value category of the expression used, not by anything
about the object itself.
*/
struct Widget {
    Widget() { std::cout << "constructed\n"; }
    Widget(const Widget&) { std::cout << "copy constructor\n"; }
    Widget(Widget&&) noexcept { std::cout << "move constructor\n"; }
};

Widget make_widget() { return Widget{}; }

int main() {
    Widget a;
    Widget b = a;              // lvalue, has identity, binds to the copy constructor
    Widget c = std::move(a);   // xvalue, same object as a, flagged as expiring
    Widget d = make_widget();  // prvalue, no persistent identity, constructed in place
}
