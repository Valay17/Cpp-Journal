/*
   Not meant to be run. Compiled to assembly and inspected with objdump,
   to see what [[likely]] and [[unlikely]] actually change.

   [[likely]] and [[unlikely]] look like they talk to the branch
   predictor directly. They do not. The predictor is pure hardware and
   learns purely from runtime behavior, nothing in source code reaches
   it. These attributes only tell the compiler how to lay out the
   generated code: the likely path goes inline, in a straight line with
   no jump needed to reach it, and the unlikely path gets moved out of
   line, reached only through a jump. That layout can help the predictor
   indirectly, a straight-line fallthrough is generally easier to predict
   well than a path requiring a taken jump, but the attribute itself is
   a hint to the compiler's code layout decisions, not an instruction the
   CPU's prediction hardware ever sees.
*/

volatile int sink;

int classify_likely(int x) {
    if (x > 0) {
        sink = 1;
        return 1;
    } else [[likely]] {
        sink = 2;
        return -1;
    }
}

int classify_no_hint(int x) {
    if (x > 0) {
        sink = 1;
        return 1;
    } else {
        sink = 2;
        return -1;
    }
}