#include <iostream>
#include <cstdlib>

bool will_overflow(int x) {
    if (x + 1 < x) {
        return true;
    }
    return false;
}

int main(int argc, char** argv) {
    int x = std::atoi(argv[1]);
    std::cout << will_overflow(x) << "\n";
}