#include <vector>

/*
This file is meant to fail to compile. That failure is the demonstration.
make_vector returns the vector itself instead of a plain value computed
from it, so the heap allocation made inside is still alive at the moment
the function returns. Trying to keep that vector alive as a constexpr
variable asks the allocation to survive past the point where compile time
ends, which is exactly the one thing constexpr heap use is not allowed to
do.
*/

constexpr std::vector<int> make_vector(int n) {
    std::vector<int> v;
    for (int i = 0; i < n; i++) v.push_back(i);
    return v;
}

int main() {
    constexpr std::vector<int> result = make_vector(5);
    return result.size();
}
