# new, operator new, Placement new, and malloc

Full writeup: https://valay17.github.io/Portfolio/blog/memory/allocation-vs-construction

There are four ways to get memory in C++, and most people only ever use one. Then they write a custom allocator or a memory pool, and the difference between "allocate" and "construct" becomes the entire problem.

Regular `new` does two things in one call: it allocates memory and constructs an object in it. One keyword, two separate jobs happening back to back. It's freed with `delete`, which also does two things, destroy the object, then release the memory.

`operator new` is the half that only allocates. It returns `void*`, raw, uninitialized memory, no constructor runs. This is what regular `new` calls internally to get its memory in the first place. Because it's a function, not a language keyword, it can be overloaded per class, redirecting a type's allocations to a custom pool without the calling code changing a single line.

Placement new is the other half, construction only, no allocation at all. It takes memory that already exists, a buffer, a pool slot, wherever, and builds an object directly inside it. Since nothing was allocated, `delete` can't be used to clean up, the destructor has to be called explicitly instead. This is the exact mechanism every memory pool relies on, allocate one big block up front, then use placement new to construct objects into pre-reserved slots as needed.

`malloc`, the C function underneath all of this, only ever does raw byte allocation. No constructor, ever, which means it fundamentally can't be used to create C++ objects, only reserve space for them. It fails by returning `nullptr` instead of throwing, and it's freed with `free`, never `delete`.

Once allocation and construction are separated like this, a pattern falls out naturally: allocate a large block once, then construct and destroy objects inside it repeatedly, without ever going back to the OS again. That's the entire model behind memory pools, arena allocators, and any lock-free structure that needs to avoid `malloc` sitting in its hot path.

## What the code demonstrates

Two files. `main.cpp` runs all four mechanisms back to back against the same instrumented type, `Widget` prints from its own constructor and destructor, so which combination of allocate/construct/destruct/deallocate actually happened is visible directly in the program's output. `overload.cpp` shows the redirection claim specifically, a class-level `operator new`/`operator delete` overload intercepting ordinary `new`/`delete` syntax at the call site without that syntax changing at all.

## Key insight

```cpp
Widget* a = new Widget();        // allocates AND constructs
delete a;                         // destructs AND deallocates

void* raw = operator new(sizeof(Widget));  // allocates only
operator delete(raw);                       // deallocates only

alignas(Widget) char buffer[sizeof(Widget)];
Widget* p = new (buffer) Widget();  // constructs only, in existing memory
p->~Widget();                        // must destruct manually, no delete

Widget* m = (Widget*)malloc(sizeof(Widget));  // raw bytes only, no constructor
free(m);                                       // raw bytes only, no destructor
```

Four mechanisms, four different combinations of allocate and construct, only one of them, plain `new`, bundles both jobs into a single call.

## Run: main.cpp

```bash
g++ -O2 -std=c++26 main.cpp -o main
./main
```
Expect `Widget constructed` and `Widget destructed` to appear for the `new`/`delete` section and the placement new section, and to be completely absent from the `operator new`/`operator delete` and `malloc`/`free` sections, confirming exactly which mechanisms touch the constructor and destructor and which don't.

## Run: overload.cpp

```bash
g++ -O2 -std=c++26 overload.cpp -o overload
./overload
```
Expect the custom `operator new` and `operator delete` messages to appear even though `main` uses plain `new PooledWidget()` and `delete w` syntax, confirming the redirection happens transparently at the class level.

## Output

```
$ ./main
=== new / delete: allocates and constructs, destructs and deallocates ===
  Widget constructed
  Widget destructed
=== operator new / operator delete: allocation only, no constructor or destructor ===
  (no constructor ran)
  (no destructor ran)
=== placement new: construction only, no allocation ===
  Widget constructed
  (no allocation happened, buffer already existed)
  Widget destructed
=== malloc / free: raw bytes only, never touches a constructor or destructor ===
  (no constructor ran)
  (no destructor ran)
```
Each mechanism's output matches exactly what its combination of allocate and construct should produce, `Widget constructed`/`Widget destructed` appear only where a constructor and destructor were actually invoked, and are completely absent from the `operator new` and `malloc` sections.

```
$ ./overload
=== plain 'new PooledWidget()', calling code unchanged ===
  custom operator new called, size=1
  PooledWidget constructed
  PooledWidget destructed
  custom operator delete called
```
`size=1` since `PooledWidget` has no data members, an empty class still needs a minimum allocation size. The call site never changed, plain `new PooledWidget()` and `delete w`, but both custom overloads fired anyway, confirming the redirection is entirely transparent to calling code.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 16.1.0
