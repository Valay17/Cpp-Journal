#include <cstdio>
#include <cstdlib>

/*
Converting a double to an int drops the fraction. If the value is too big
for an int, even after dropping the fraction, the behavior is undefined.
The value comes from the command line so the compiler cannot fold the
conversion at compile time. Built with -fsanitize=float-cast-overflow the
conversion is reported when it happens.
*/

int main(int argc, char** argv) {
    double d = argc > 1 ? atof(argv[1]) : 1e10;
    int t = d;
    printf("int t = %g -> %d\n", d, t);
}
