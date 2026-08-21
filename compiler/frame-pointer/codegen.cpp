/*
Not run, compiled and disassembled only, at default -O2 and at
-O2 -fno-omit-frame-pointer. bar is left undefined and external on purpose,
so the compiler cannot inline it away, the point is what foo's own frame
setup looks like around two calls to something it cannot see inside of.
*/

extern int bar(int x);

int foo(int a) {
    int b = a + 10;
    return bar(b) + bar(b + 1);
}
