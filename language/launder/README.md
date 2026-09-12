# std::launder: Correct Per the Standard, Not Per What Breaks

Full writeup: https://valay17.github.io/Portfolio/blog/language/launder

Casting a pointer to a different type is never itself undefined behavior. Reading through it can be. `std::launder` is supposed to fix that. Or does it?

Pointers carry more than an address. The compiler tracks what it calls provenance, which object a pointer actually came from, and that tracking exists so the compiler can safely cache values, skip redundant reloads, and reorder memory operations without breaking correctness. `reinterpret_cast` changes how you're allowed to interpret a pointer's bits. It does nothing to fix provenance. Two pointers holding the same address can still point at objects the compiler treats as unrelated.

This becomes a problem the moment you destroy an object and construct a new one in the same storage, the pattern behind every memory pool that reuses a slot without a full deallocation. Destroy a `float` living in a buffer, placement-new an `int` into that same buffer, and any pointer obtained before the destruction now points at storage the compiler no longer associates with a live object of that type, even though the bytes and address never moved. `std::launder` fixes this directly, it reestablishes provenance, forcing the compiler to treat the returned pointer as pointing at whatever object is sitting there now, instead of trusting a cached assumption from before. It doesn't cast anything and it can't create an object that isn't already there, laundering a pointer to a type that was never constructed in that storage is still undefined behavior.

There's also a narrower case where you don't need it at all. If the new object is the same type, occupies the same storage, has no const subobjects, and involves no base-class subobject trickery, the standard calls this transparently replaceable, and the original pointer stays valid without laundering. Miss any one of those four conditions, and you're back to needing it explicitly.

## Trying to make this visibly break, and why it didn't

Calling something undefined behavior means nothing if nobody tries to actually trigger it. So I did, repeatedly, before writing any of this up.

Tried, in order, at `-O0` through `-O3`, on GCC 13.3.0 and GCC 16.1.0:

1. Destroying a `float` in a buffer and placement-newing an `int` into that same storage.
2. The textbook `const`-member case, the actual motivating example from the C++17 proposal that introduced `std::launder`.
3. That same case forced through an opaque `noinline` function boundary.
4. That same case fully inline, maximum compiler visibility, no call boundary at all.
5. A loop reconstructing the object five times, testing whether loop-invariant code motion would hoist a stale read out of the loop body.
6. Polymorphic reconstruction, a different derived type placement-newed into the same storage, testing whether devirtualization would call the wrong derived implementation through the stale pointer.
7. Two virtual calls back to back on the same stale pointer in one expression, maximizing the chance of common subexpression elimination reusing a cached vtable load.
8. The `const`-member case repeated with heap allocation instead of a stack buffer, ruling out stack locality as the reason nothing diverged.
9. The virtual dispatch case repeated with heap allocation and opaque, `noinline` construction functions.
10. The strongest combination available: separate translation units, heap allocation, a `const` member, and link-time optimization, giving the compiler cross-unit visibility and every optimization opportunity it has.
11. A specific "different provenance" example, `const int* ptr = new const int(11)`, destroyed and reconstructed in place as a plain `int`, a case commonly used to illustrate provenance-based UB directly. Worth noting this example did not even compile as commonly written, `static_cast<void*>` from a `const int*` is illegal, a `const_cast` was needed to test the actual intent.

Every single one produced the correct result regardless of whether `std::launder` was used. Confirmed independently on two different machines running the two different GCC versions above, not just once.

This turns out not to be a coincidence or a quirk of one compiler. **WG21 paper P3006R1, "Launder less,"** proposes removing this exact UB from the standard, and states directly that popular compilers do not require `std::launder` in this situation and produce the expected assembly without it, because no default-enabled optimization in practice relies on this specific UB. The paper backs this with named, shipping code that has skipped `std::launder` here since before it existed: `boost::optional`'s aligned storage, the userver framework's `FastPimpl`, ClickHouse's `HashTable`, libcxx's own `function.h`, and V8's lazy instance initialization, all cited directly in the proposal as evidence the gap doesn't matter in practice. The same paper notes Clang has one relevant optimization, `-fstrict-vtable-pointers`, that could exploit `std::launder` violations for polymorphic types, and it is disabled by default specifically because enabling it breaks too much existing code. A Boost mailing list thread makes the same point from the library-author side, `Boost.Optional` and `Boost.Variant` have technically violated this rule since C++03 and "worked fine all that time," described in that thread as UB that "existed only 'on paper'."

That is the actual finding, not a failed demonstration, and it now has a confirmed counterpart rather than just a documented possibility. `std::launder`'s necessity comes from what the standard says about object lifetime and pointer provenance, not from whether a specific compiler currently exploits the gap. Undefined behavior does not require a compiler to do anything different, it only permits it to, and Clang's `-fstrict-vtable-pointers`, confirmed below, is proof that permission gets used the moment the right optimization is switched on. Code relying on this working, and it does work by default across every GCC pattern tested here, is not correct, it's unexploited under the compilers and flags currently in wide use. A future GCC release, a different default configuration, or simply building with the one flag that already exists could break it without a single line of the code itself changing.

## The one place this actually breaks

Every attempt against GCC came back clean. Testing against Clang with the one documented mechanism built specifically for this, `-fstrict-vtable-pointers`, confirmed to exist for exactly this purpose by P3006R1 above, just disabled by default, broke immediately.

```cpp
// vtable_strict.cpp
#include <new>
#include <iostream>

struct Base {
    virtual int id() const { return 0; }
    virtual ~Base() = default;
};
struct DerivedA : Base { int id() const override { return 1; } };
struct DerivedB : Base { int id() const override { return 2; } };

int main() {
    alignas(DerivedA) unsigned char buf[sizeof(DerivedA) > sizeof(DerivedB) ? sizeof(DerivedA) : sizeof(DerivedB)];
    Base* p = new (buf) DerivedA();
    p->~Base();
    new (buf) DerivedB();
    std::cout << p->id() << "\n";               // no launder
}
```

```bash
clang++ -O2 -fstrict-vtable-pointers -std=c++26 vtable_strict.cpp -o vtable_strict
./vtable_strict
```

Prints `1`. `p` was constructed as `DerivedA`, destroyed, and a `DerivedB` was placement-newed into the same storage. The object sitting there right now is a `DerivedB`, `id()` should return `2`. It returns `1` instead, `DerivedA`'s answer, because `-fstrict-vtable-pointers` devirtualized the call based on the stale provenance from the original construction rather than actually reading the vtable pointer at the call site.

```cpp
// vtable_strict_fixed.cpp
#include <new>
#include <iostream>

struct Base {
    virtual int id() const { return 0; }
    virtual ~Base() = default;
};
struct DerivedA : Base { int id() const override { return 1; } };
struct DerivedB : Base { int id() const override { return 2; } };

int main() {
    alignas(DerivedA) unsigned char buf[sizeof(DerivedA) > sizeof(DerivedB) ? sizeof(DerivedA) : sizeof(DerivedB)];
    Base* p = new (buf) DerivedA();
    p->~Base();
    new (buf) DerivedB();
    std::cout << std::launder(p)->id() << "\n"; // laundered
}
```

```bash
clang++ -O2 -fstrict-vtable-pointers -std=c++26 vtable_strict_fixed.cpp -o vtable_strict_fixed
./vtable_strict_fixed
```

Prints `2`. Identical code, the only change is `std::launder(p)` in place of `p`, and the answer flips from wrong to correct. Not run in this repo's own GCC-only environment, this pair was run with Clang elsewhere, but the result is a confirmed reproduction, not a hypothetical: the UB is standard-mandated, exploited by an actual optimization flag, and `std::launder` is what fixes it when that flag is on.

This changes what the GCC attempts mean. They aren't evidence the UB doesn't matter, they're evidence that GCC, as currently implemented, chooses not to exploit something Clang already can, and does, the moment the right flag is flipped. `-fstrict-vtable-pointers` being off by default is a policy decision about breaking existing code today, not a statement that the optimization doesn't work or couldn't ship on by default in a future release.

## What the code demonstrates

`main.cpp` contains four single-file patterns: the float-to-int example, the `const`-member case, the loop-based reconstruction, and polymorphic reconstruction with devirtualization. `lib.cpp` and `lto_main.cpp` together are the strongest single-run attempt, separate translation units linked with `-flto`, since that pattern isn't reproducible in a single file. The image-derived "different provenance" example is documented above but not shipped as its own file, since it's a variation on the same `const`-member mechanism already covered by `main.cpp`. `vtable_strict.cpp` and `vtable_strict_fixed.cpp` are the confirmed break, Clang's `-fstrict-vtable-pointers` devirtualizing based on stale provenance, and `std::launder` fixing it, both requiring Clang specifically, not reproducible with this repo's GCC toolchain.

## Key insight

```cpp
alignas(int) unsigned char buf[sizeof(int)];
using FloatT = float;

float* pf = new (buf) float(3.14f);
pf->~FloatT();

int* pi = new (buf) int(69);          // new object, same storage, different type

unsigned char* raw = buf;
int* stale = reinterpret_cast<int*>(raw);
// *stale;                             // UB: reinterpret_cast doesn't restore provenance

int* fixed = std::launder(stale);
std::cout << *fixed;                   // OK: provenance re-established, reads 69
```

`reinterpret_cast` changes the type the pointer is interpreted as. It does nothing about which object the compiler believes the pointer refers to. `std::launder` is the only one of the two that addresses provenance at all.

## Run: main.cpp

```bash
g++ -O2 -std=c++26 main.cpp -o main
./main
```
Expect all four functions to print correct, matching values whether or not `std::launder` was used in each. That agreement is the confirmed, expected result, not a sign the demonstration failed.

## Run: LTO cross-translation-unit build

```bash
g++ -O2 -flto -std=c++26 lib.cpp lto_main.cpp -o lto_test_O2
./lto_test_O2
g++ -O3 -flto -std=c++26 lib.cpp lto_main.cpp -o lto_test_O3
./lto_test_O3
```
`-flto` enables link-time optimization, giving the compiler visibility across `lib.cpp` and `lto_main.cpp` as if they were one file, the most aggressive optimization context available for this test. Expect `a=1 b=2 c=2` at both optimization levels, matching the non-LTO results exactly.

## Run: the confirmed break, Clang only

```bash
clang++ -O2 -fstrict-vtable-pointers -std=c++26 vtable_strict.cpp -o vtable_strict
./vtable_strict
clang++ -O2 -fstrict-vtable-pointers -std=c++26 vtable_strict_fixed.cpp -o vtable_strict_fixed
./vtable_strict_fixed
```
Requires Clang, not reproducible with this repo's GCC toolchain, GCC has no equivalent flag. `-fstrict-vtable-pointers` is the one documented optimization P3006R1 names as capable of exploiting this UB, and it's off by default specifically because turning it on breaks existing code. Expect `vtable_strict` to print `1`, the wrong answer, `DerivedA`'s `id()` instead of the actually-constructed `DerivedB`'s. Expect `vtable_strict_fixed`, identical except for one added `std::launder`, to print `2`, the correct answer.

## Output

```
$ ./main
float_to_int_example: via reinterpret_cast (UB) = 69, via std::launder = 69
const_member_example: a=1 b=2 c=2
loop_example (no launder): 0 1 2 3 4
virtual_dispatch_example: first=1 no_launder=2 with_launder=2 direct=2

$ ./lto_test_O2
a=1 b=2 c=2

$ ./lto_test_O3
a=1 b=2 c=2

$ clang++ -O2 -fstrict-vtable-pointers -std=c++26 vtable_strict.cpp -o vtable_strict
$ ./vtable_strict
1

$ clang++ -O2 -fstrict-vtable-pointers -std=c++26 vtable_strict_fixed.cpp -o vtable_strict_fixed
$ ./vtable_strict_fixed
2
```
Every attempt against GCC 13.3.0 and GCC 16.1.0 produced correct output regardless of `std::launder`. Against Clang with `-fstrict-vtable-pointers`, the unlaundered version broke immediately, `1` instead of the correct `2`, and read correctly the moment `std::launder` was added back in. This UB is standard-mandated, unexploited by GCC as currently implemented, and confirmed exploited by Clang under one specific, currently off-by-default flag.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 16.1.0
- Compiler (Clang-only tests, `vtable_strict.cpp` / `vtable_strict_fixed.cpp`): clang version 21.1.8
