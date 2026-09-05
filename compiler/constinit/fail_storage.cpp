/*
This file is meant to fail to compile. constinit only applies to
variables with static or thread-local storage duration. local has
automatic storage duration, an ordinary stack variable, so constinit is
rejected here regardless of whether its initializer is a compile-time
constant.
*/

constexpr int get_value() { return 69; }

void foo() {
    constinit int local = get_value();
}
