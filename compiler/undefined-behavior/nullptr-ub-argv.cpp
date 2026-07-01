#include <iostream>
#include <cstdlib>

int read_value(int* ptr) {
    int val = *ptr;
    if (ptr == nullptr) {
        return -1;
    }
    return val;
}

int main(int argc, char** argv) {
    int* ptr = reinterpret_cast<int*>(std::atol(argv[1]));
    std::cout << read_value(ptr) << "\n";
}