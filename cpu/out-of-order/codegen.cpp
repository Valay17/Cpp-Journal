/*
Not run, compiled and disassembled only, to check whether the compiler
actually schedules independent work around a slow operation the way the
post's snippet implies, and whether that depends on what kind of
operation "slow" is.
*/

extern long slow_function(long x); // opaque, defined elsewhere, never inlined

// a call is a hard control-flow boundary. Instructions physically located
// after it in the binary are not even fetched until the call returns, so
// there is nothing for the compiler, or the CPU, to schedule ahead of it.
long compute_call(long x, long y, long z, long w) {
    long a = slow_function(x);
    long b = (y * 2 + 7) ^ (z * 3 - w) + (y & z) * (w | y);
    return a + b;
}

// a load is not a control-flow boundary. Both the compiler statically and
// the CPU dynamically are free to schedule independent work around it.
long compute_load(const long* arr, long x, long y, long z, long w) {
    long a = arr[x];
    long b = (y * 2 + 7) ^ (z * 3 - w) + (y & z) * (w | y);
    return a + b;
}
