#include <vector>
#include <iostream>

/*
std::vector allocates on the heap internally, but every element pushed
here is read back out and folded into a single int before the function
returns. No allocation survives past the return statement, which is
exactly the restriction that makes this legal at compile time.
*/
constexpr int sum_via_vector(int n) {
    std::vector<int> v;
    for (int i = 0; i < n; i++) v.push_back(i);
    int total = 0;
    for (int x : v) total += x;
    return total;
}

int main() {
    // constexpr here forces compile-time evaluation, not just permits it
    constexpr int result = sum_via_vector(5);
    static_assert(result == 10); // fails to compile if this did not run at compile time
    std::cout << "sum_via_vector(5) = " << result << "\n";
}
