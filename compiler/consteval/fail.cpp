/*
This file is meant to fail to compile. That failure is the demonstration.
square is consteval, an immediate function, x is a runtime parameter with
no compile-time-known value, so this call cannot be resolved ahead of
time. Unlike a plain constexpr function, there is no fallback to a
runtime path, the build simply fails.
*/

consteval int square(int n) { return n * n; }

int runtime_call(int x) {
    return square(x);
}
