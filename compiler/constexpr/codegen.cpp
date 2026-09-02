/*
Not run, compiled and disassembled only. Same function, called two ways,
to show that constexpr changes what a call is allowed to do, not what it
always does.
*/

constexpr int square(int n) { return n * n; }

// x is a runtime parameter, unknown at compile time, so this call has to
// actually happen when the program runs
int runtime_call(int x) {
    return square(x);
}

// result is declared constexpr, which forces this call into a context that
// requires a compile-time value, so the entire computation happens before
// the program ever runs
int compile_time_call() {
    constexpr int result = square(5);
    return result;
}
