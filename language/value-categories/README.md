# Value Categories: What Decides Copy vs Move

Full writeup: https://valay17.github.io/Portfolio/blog/language/value-categories

The same variable, written twice in the same function, can trigger two different constructors. Nothing about the variable changed. What changed is how it was used.

Every C++ expression has a value category, and it isn't about the type or the variable, it's about how that specific expression is being used at that specific point in the code. A named variable used plainly is an lvalue, it has identity, a memory location that persists. Wrap that same variable in `std::move`, and the expression becomes an xvalue, still the same object, same memory location, but now flagged as expiring, safe to steal resources from. A temporary, like the direct result of a function returning by value, is a prvalue, it has no persistent identity at all.

This isn't academic. It directly decides which constructor overload gets called. Pass a variable plainly, and overload resolution picks the copy constructor, since an lvalue can't bind to an rvalue reference. Wrap the exact same variable in `std::move`, and the xvalue it produces binds to the move constructor instead, the compiler picks a completely different function based purely on how the expression was categorized, not on what type the variable is.

Prvalues get treated differently still. Since C++17, when a prvalue is used to initialize an object, there's no temporary built somewhere else and then copied or moved into place, the standard mandates the object gets constructed directly where it's needed. Not an optimization the compiler happens to apply, a guarantee. Even with copy elision explicitly disabled at the compiler flag level, a prvalue initializing a variable skips both the copy and move constructor entirely, there's nothing to elide, because nothing extra was ever created in the first place.

## What the code demonstrates

One file, one struct, three initializations of the same type from the same original object, each using a different value category. `Widget` prints from each of its constructors, so which one fires is visible directly in the program's own output, no disassembly needed to see the difference.

Compiled and run two ways, once normally and once with `-fno-elide-constructors`, the compiler flag that disables the older, optional kind of copy elision. Both builds produce identical output, confirming the `make_widget()` case is not benefiting from an optimization that flag could have disabled, it's the C++17 mandatory elision rule, which applies regardless of that flag.

## Key insight

```cpp
struct Widget {
    Widget() { std::cout << "constructed\n"; }
    Widget(const Widget&) { std::cout << "copy constructor\n"; }
    Widget(Widget&&) noexcept { std::cout << "move constructor\n"; }
};

Widget make_widget() { return Widget{}; }

int main() {
    Widget a;
    Widget b = a;              // lvalue -> copy constructor
    Widget c = std::move(a);   // xvalue -> move constructor
    Widget d = make_widget();  // prvalue -> neither, constructed in place
}
```

`a` is the same object in both `b`'s and `c`'s initialization. Only the expression wrapping it differs.

## Run: main.cpp

```bash
g++ -O2 -std=c++26 main.cpp -o main
./main
```
Expect `constructed`, `copy constructor`, `move constructor`, `constructed`, in that order, one line per constructor call.

## Run: with elision explicitly disabled

```bash
g++ -O2 -std=c++26 -fno-elide-constructors main.cpp -o main-noelide
./main-noelide
```
`-fno-elide-constructors` disables the compiler's discretionary copy elision, the kind that was always an optimization, never a guarantee. Expect byte-for-byte identical output to the normal build, confirming `d`'s construction was never relying on that discretionary optimization in the first place, the C++17 mandatory rule for prvalues applies with or without this flag.

## Output

```
$ ./main
constructed
copy constructor
move constructor
constructed

$ ./main-noelide
constructed
copy constructor
move constructor
constructed
```
Identical, line for line, between both builds. `d`'s construction skips both the copy and move constructor either way, confirming it was never the discretionary kind of elision `-fno-elide-constructors` can disable, it's the C++17 mandatory rule holding regardless of that flag.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 16.1.0
