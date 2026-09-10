# Type Erasure: The Mechanism Behind std::function

Full writeup: https://valay17.github.io/Portfolio/blog/templates/type-erasure

`std::function` can hold a lambda, a raw function pointer, or a functor object, three unrelated types, with no shared base class between any of them. Here's the mechanism that actually makes that possible.

Virtual functions solve the "call the right implementation for different types" problem by having the compiler build a vtable, a hidden array of function pointers attached to any object with a virtual method, resolved through an extra pointer indirection at the call site. Type erasure gets the same runtime behavior without any of that machinery. A wrapper class stores a `void*` to the actual object, plus its own function pointers pointing at the exact operations that type needs, call, destroy, copy, whatever the interface requires. No inheritance, no virtual keyword, no compiler-generated vtable, just function pointers stored by hand.

The construction happens through a template constructor, which is the part that makes this work across unrelated types. When the wrapper is built from one type, the template instantiates function pointers specific to that exact type, a lambda that knows precisely how to cast back and call it correctly. Build the same wrapper from a different type, and a different set of function pointers gets generated. The wrapper's own type never changes, only the function pointers stored inside it do, which is what lets unrelated types live behind one uniform interface.

This is exactly the mechanism `std::function` and `std::any` run underneath their public interface, a uniform, non-templated type that can hold anything satisfying a behavioral contract. Every call still costs an indirect function call, the same order of cost as a virtual call, type erasure isn't free, it's just built by hand instead of by the compiler.

## What the code demonstrates

One file, one hand-rolled wrapper, `Shape`, holding two unrelated types, `Circle` and `Square`, with no common base class and no virtual functions declared anywhere. Confirmed three separate ways: the program dispatches to the correct type's `draw()` for each element, the symbol table contains zero vtable symbols but a distinct compiler-generated function-pointer target per type the wrapper was built from, and the actual call inside `Shape::draw()` disassembles to an indirect call through a register, `call rdx`, holding whichever function pointer this particular instance was constructed with.

## Key insight

```cpp
class Shape {
    void* data;
    void (*draw_fn)(const void*);
    void (*destroy_fn)(void*);

public:
    template<typename T>
    Shape(T obj)
        : data(new T(std::move(obj)))
        , draw_fn([](const void* p) { static_cast<const T*>(p)->draw(); })
        , destroy_fn([](void* p) { delete static_cast<T*>(p); })
    {}

    void draw() const { draw_fn(data); }
    ~Shape() { destroy_fn(data); }
};
```

`Shape` itself is never a template, `T` only appears inside the constructor. Every `Shape` instance has the identical type regardless of what it was built from, only the function pointers it's carrying differ.

## Run: main.cpp

```bash
g++ -O2 -std=c++26 main.cpp -o main
./main
```
Expect `Circle::draw` followed by `Square::draw`, confirming correct dispatch for both unrelated types through the same `Shape` interface.

## Run: confirm no vtable exists

```bash
nm main | grep -i vtable
```
This is expected to return nothing. No match confirms there is no vtable symbol anywhere in the binary, `Shape` never uses the `virtual` keyword, so the compiler never generates one.

```bash
nm main | c++filt | grep -i shape
```
Expect distinct `{lambda...}::_FUN` symbols per type the wrapper was constructed with, one set for `Shape::Shape<Circle>`, a separate set for `Shape::Shape<Square>`, direct evidence that the template constructor generates different function pointers per type rather than sharing one implementation.

## Run: confirm the indirect call

```bash
g++ -O0 -std=c++26 -c main.cpp -o main_o0.o
objdump -d -M intel --no-show-raw-insn main_o0.o | c++filt | grep -A 16 "^0000000000000000 <Shape::draw"
```
`-O0` keeps the function un-inlined and easy to read, since the point here is seeing the call mechanism, not the optimized result. Expect the disassembly to load the stored function pointer from the object and end in `call` through a register, not a direct call to a fixed address, confirming this dispatches the same way a virtual call would at the hardware level, just without a vtable involved.

## Output

```
$ ./main
Circle::draw
Square::draw
```

```
$ nm main | grep -i vtable
(no output)
```
Nothing found, no vtable symbol exists anywhere in the binary.

```
$ nm main | c++filt | grep -i shape
00000000000012b0 W Shape::Shape<Circle>(Circle)::{lambda(void const*)#1}::_FUN(void const*)
0000000000001270 W Shape::Shape<Circle>(Circle)::{lambda(void*)#1}::_FUN(void*)
00000000000012d0 W Shape::Shape<Square>(Square)::{lambda(void const*)#1}::_FUN(void const*)
0000000000001290 W Shape::Shape<Square>(Square)::{lambda(void*)#1}::_FUN(void*)
```
Two complete, separate sets of function pointer targets, one for `Shape<Circle>`, one for `Shape<Square>`, each pair generated independently by the template constructor for the exact type it was instantiated with.

```
$ objdump -d -M intel --no-show-raw-insn main_o0.o | c++filt | grep -A 16 "^0000000000000000 <Shape::draw"

0000000000000000 <Shape::draw() const>:
   0:   push   rbp
   1:   mov    rbp,rsp
   4:   sub    rsp,0x10
   8:   mov    QWORD PTR [rbp-0x8],rdi
   c:   mov    rax,QWORD PTR [rbp-0x8]
  10:   mov    rdx,QWORD PTR [rax+0x8]
  14:   mov    rax,QWORD PTR [rbp-0x8]
  18:   mov    rax,QWORD PTR [rax]
  1b:   mov    rdi,rax
  1e:   call   rdx
  20:   nop
  21:   leave
  22:   ret
```
The stored function pointer is loaded from the object at `[rax+0x8]` into `rdx`, and the call is `call rdx`, an indirect call through a register, not a call to a fixed address. Same dispatch cost as a virtual call at the hardware level, built entirely by hand.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 16.1.0
