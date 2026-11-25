#include <cstdio>
#include <vector>

/*
p points at the third element, 3. After erasing the first element, every
element after it moves down one slot, so the slot p points at now holds 4.
p is invalid after the erase, but the slot is still inside the vector's
memory, so reading it touches nothing freed. There is no crash and nothing
for an address sanitizer to report, only the wrong element.
*/

int main() {
    std::vector<int> v{1, 2, 3, 4};
    auto p = v.begin() + 2;                 // points at 3
    v.erase(v.begin());
    int x = *p;                             // 4: no crash, no report, wrong element
    printf("x = %d\n", x);
}
