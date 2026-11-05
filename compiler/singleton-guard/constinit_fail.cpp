/*
Same shape, but Plain's constructor is not constexpr, so it cannot run at
compile time and the variable needs the guard. constinit turns that into a
compile error instead of a silent guard on every call.
*/

struct Plain {
    int value;
    Plain() : value(69) {}
};

Plain& plain_instance() {
    static constinit Plain c;
    return c;
}

int main() { return plain_instance().value == 69 ? 0 : 1; }
