#include <xvec/simd>

/*
This file is not run. It is compiled to an object file and inspected with
objdump, to check what instruction vec<float,8> multiplication actually
lowers to.

scale_by_two takes an 8-lane float vector by value and multiplies it by the
constant 2.0f, matching the post's example. scale_by_three multiplies by
3.0f instead, as a control, to check whether whatever instruction shows up
for scale_by_two is how vec<float,8> multiplication works in general, or
specific to multiplying by 2. add_vecs adds two vectors directly, to compare
against whatever scale_by_two produces.
*/

using xvec::simd::vec;

vec<float, 8> scale_by_two(vec<float, 8> values) {
    return values * vec<float, 8>(2.0f);
}

vec<float, 8> scale_by_three(vec<float, 8> values) {
    return values * vec<float, 8>(3.0f);
}

vec<float, 8> add_vecs(vec<float, 8> a, vec<float, 8> b) {
    return a + b;
}
