#include <array>
#include <vector>
#include <iostream>

/*
operator[] does no bounds checking on std::array, same as a plain C array.
.at() does, and throws std::out_of_range instead of silently writing
somewhere invalid. The same gap exists on std::vector, included here to
show this isn't specific to std::array, it is a general operator[] vs .at()
distinction across the standard containers that support both.
*/

int main() {
    std::array<int, 10> arr{};
    arr[500] = 1; // out of bounds, no check, may not even crash
    std::cout << "arr.size() after OOB write via operator[]: " << arr.size() << "\n";

    try {
        arr.at(500) = 1;
        std::cout << "no exception thrown (unexpected)\n";
    } catch (const std::out_of_range& e) {
        std::cout << "caught: " << e.what() << "\n";
    }

    std::vector<int> vec(10);
    vec[500] = 1; // same gap on std::vector
    std::cout << "vec.size() after OOB write via operator[]: " << vec.size() << "\n";

    try {
        vec.at(500) = 1;
        std::cout << "no exception thrown (unexpected)\n";
    } catch (const std::out_of_range& e) {
        std::cout << "caught: " << e.what() << "\n";
    }
}
