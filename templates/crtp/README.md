# CRTP: The Pattern Already Inside the Standard Library

Full writeup: https://valay17.github.io/Portfolio/blog/templates/crtp

`std::enable_shared_from_this`, something almost everyone reaches for without a second thought, is CRTP. The exact pattern people call "advanced" and avoid is already sitting inside the standard library, used constantly by people who've never heard the term.

CRTP, the Curiously Recurring Template Pattern, is a class deriving from a template instantiated with itself, `class Circle : public Shape<Circle>`. That looks circular because it is, and it's exactly what makes the pattern work. The base class template can call methods on the derived type through a `static_cast`, `static_cast<Derived*>(this)->some_method()`, resolved entirely at compile time, because the compiler already knows the concrete type at the point of instantiation.

That's the entire mechanism behind what people call static or compile-time polymorphism, the same shape as virtual dispatch, a base type exposing an interface that derived types implement differently, but with none of a vtable's machinery. Confirmed directly, no vtable pointer load, no indirect call through a function pointer, nothing. The function that looked like it went through a base class interface compiles down to the derived type's actual logic, inlined directly, as if the abstraction was never there at all.

C++23 added explicit object parameters, "deducing this," which gives the same pattern a cleaner path. A member function can take the object itself as an explicit, deducible parameter, `auto area(this auto&& self)`, letting the derived type get deduced automatically at the call site instead of needing the template base class and the manual `static_cast`.

The tradeoff CRTP carries either way is the one you'd expect from anything resolved at compile time instead of runtime. You can't hold a collection of different derived types behind a single base pointer and call the same function polymorphically at runtime, the compiler needs to know the concrete type ahead of time, which means CRTP fits when the set of types is fixed and known upfront, not when you need runtime substitutability.

## A note on compiler version

`this auto&& self` requires a compiler with C++23 "deducing this" support. GCC added this relatively recently, I am using GCC 16.1.0, well past the point it landed, but if this is being built on an older GCC, the classic CRTP half will still compile fine on its own, the deducing-this half specifically needs a current compiler.

## What the code demonstrates

One file, two versions of the same shape, `Shape`/`Circle` using classic CRTP, `ShapeNew`/`CircleNew` using deducing this. Both compute the same area formula through what looks like a base-class interface call. `compute_classic` and `compute_new` exist as free functions specifically to give each version a clean, individually disassemblable symbol, so the generated code for each can be compared directly.

## Key insight

```cpp
template<typename Derived>
struct Shape {
    double area() const {
        return static_cast<const Derived*>(this)->area_impl();
    }
};

struct Circle : Shape<Circle> {
    double r;
    double area_impl() const { return 3.14159 * r * r; }
};

struct ShapeNew {
    auto area(this auto&& self) {
        return self.area_impl();
    }
};

struct CircleNew : ShapeNew {
    double r;
    double area_impl() const { return 3.14159 * r * r; }
};
```

Same shape, two different ways to get there, one needs the template base and the manual cast, the other doesn't.

## Run: main.cpp

```bash
g++ -O2 -std=c++26 main.cpp -o main
./main
```
Expect `classic: 153.938` and `new: 153.938`, confirming both versions compute the identical result.

## Run: codegen comparison

```bash
g++ -O2 -std=c++26 -c main.cpp -o main.o
objdump -d -M intel --no-show-raw-insn main.o | grep -A 8 "compute_classic"
objdump -d -M intel --no-show-raw-insn main.o | grep -A 8 "compute_new"
```
`-c` compiles to an object file without linking, since the point here is inspecting the generated instructions, not running the binary. Expect both functions to compile to the same shape, direct floating-point multiplication, no vtable pointer load, no indirect call through a function pointer anywhere in either one.

## Output

```
$ ./main
classic: 153.938
new:     153.938
```

```
$ objdump -d -M intel --no-show-raw-insn main.o | grep -A 8 "compute_classic"

0000000000000000 <compute_classic(Circle const&)>:
   0:   movsd  xmm1,QWORD PTR [rdi]
   4:   movsd  xmm0,QWORD PTR [rip+0x0]
   c:   mulsd  xmm0,xmm1
  10:   mulsd  xmm0,xmm1
  14:   ret

$ objdump -d -M intel --no-show-raw-insn main.o | grep -A 8 "compute_new"

0000000000000020 <compute_new(CircleNew const&)>:
  20:   movsd  xmm1,QWORD PTR [rdi]
  24:   movsd  xmm0,QWORD PTR [rip+0x0]
  2c:   mulsd  xmm0,xmm1
  30:   mulsd  xmm0,xmm1
  34:   ret
```
Identical instruction sequence, `movsd` / `movsd` / `mulsd` / `mulsd` / `ret`, in both functions, just at different addresses. No vtable pointer load, no indirect call, in either version, confirmed on a compiler that actually supports deducing this, not just the classic half.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 16.1.0
