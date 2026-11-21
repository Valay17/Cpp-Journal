#include <cstdio>
#include <cstdlib>

/*
n * n is computed in int, because both operands are int, and 100000 times
100000 is 10 billion, which does not fit in an int. Signed overflow is
undefined behavior. The multiplication happens first and only then is the
result widened to long long, so the widening cannot rescue it. The fix
widens one operand before the multiplication, which makes the whole
multiplication a long long one.

n comes from the command line so the compiler cannot fold the product at
compile time and has to emit the multiplication.
*/

int main(int argc, char** argv) {
    int n = argc > 1 ? atoi(argv[1]) : 100000;
    long long wrong = n * n;                // multiplied as int (overflow), then widened
    long long right = (long long)n * n;     // widened first: 10000000000
    printf("wrong = %lld\n", wrong);
    printf("right = %lld\n", right);
}
