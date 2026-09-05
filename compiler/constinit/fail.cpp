/*
This file is meant to fail to compile. That failure is the demonstration.
bad's initializer calls an ordinary, non-constexpr function, so its value
cannot be resolved at compile time. constinit does not fall back to a
runtime initialization the way a plain variable would, the build fails
outright. good is included in the same file to show the working case
would sit right next to the failing one in ordinary code, not as something
that compiles on its own here.
*/

int get_runtime_value() { return 69; }
constexpr int get_compile_time_value() { return 69; }

constinit int bad  = get_runtime_value();
constinit int good = get_compile_time_value();

int main() {
    good = 100; // legal, constinit only locks the start
}
