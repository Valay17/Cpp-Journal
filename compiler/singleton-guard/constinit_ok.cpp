/*
constinit on the static variable asserts that it is initialized at compile
time. Const has a constexpr constructor, so this compiles, and
const_instance() has no guard, same as without the keyword. constinit adds
no behavior here. It only turns "the compiler happened to do this" into
"the compiler must do this or refuse to compile".
*/

struct Const {
    int value;
    constexpr Const() : value(69) {}
};

Const& const_instance() {
    static constinit Const c;
    return c;
}

int main() { return const_instance().value == 69 ? 0 : 1; }
