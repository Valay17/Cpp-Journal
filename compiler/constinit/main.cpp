#include <iostream>

/*
good's initializer is a constexpr function, resolvable at compile time,
so constinit accepts it. Mutating good afterward is completely ordinary,
constinit locks how the variable starts, not what it can become.
*/
constexpr int get_compile_time_value() { return 69; }
constinit int good = get_compile_time_value();

int main() {
    std::cout << "before: " << good << "\n";
    good = 100;
    std::cout << "after: " << good << "\n";
}
