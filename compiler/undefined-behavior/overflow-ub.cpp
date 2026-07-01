#include <iostream>

/*
   Signed integer overflow is undefined behavior in C++, not
   implementation-defined, not guaranteed to wrap. The standard says it
   cannot happen in a correct program, and the compiler is free to build
   on that as a hard rule. will_overflow checks for overflow using
   x + 1 < x, the classic way someone might try to catch it. Since
   x + 1 overflowing is something the standard says can never legally
   happen, the compiler is permitted to reason that the comparison is
   always false and remove it entirely, along with the function call
   site it would otherwise feed into.

   This same file is compiled three separate ways, each one a fully
   separate invocation, never combined:

   1. Default build, no special flags. Expect the check to disappear
      entirely, the function may compile down to an unconditional
      return with no comparison instruction anywhere.

   2. With -fwrapv, which tells the compiler signed overflow wraps
      around instead of being undefined. The check becomes meaningful
      again under that assumption, expect a real comparison instruction
      to reappear in the generated code.

   3. With -fsanitize=undefined (UBSan), which does not change what the
      optimizer assumes at compile time, but instruments the binary to
      detect undefined behavior at runtime and report it the moment it
      actually happens, rather than silently allowing the compiler to
      assume it never will.

   The overflow itself happens here on purpose, passing INT_MAX so that
   x + 1 genuinely overflows a 32 bit signed int. This is intentional UB
   for demonstration. It is not a pattern to write in real code.
*/

bool will_overflow(int x) {
    if (x + 1 < x) {
        return true;
    }
    return false;
}
int main() {
    std::cout << will_overflow(2147483647) << "\n";
}
