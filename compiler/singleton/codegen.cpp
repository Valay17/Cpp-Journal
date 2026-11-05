/*
This file is not run. It is compiled to an object file and inspected with
objdump and nm.

Plain and Const are the same struct with the same constructor body, value(69).
The only difference is the word constexpr on Const's constructor. A static
variable inside a function must be initialized the first time control passes
through its declaration, and the standard requires that to be thread-safe, so
Plain's variable gets a hidden guard flag checked on every call. Const's
constructor can run at compile time, so its variable is initialized before the
program starts and needs no flag.
*/

struct Plain {
    int value;
    Plain() : value(69) {}              // same body, not constexpr
};

Plain& plain_instance() {
    static Plain c;                     // guard: load, test, branch on every call
    return c;                           // first call: __cxa_guard_acquire, build, release
}

struct Const {
    int value;
    constexpr Const() : value(69) {}    // same body, constexpr
};

Const& const_instance() {
    static Const c;                     // no guard: built at compile time
    return c;
}
