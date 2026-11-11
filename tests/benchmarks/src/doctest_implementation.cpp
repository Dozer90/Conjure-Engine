// The doctest implementation for conjure_benchmarks (docs/design/08_testing.md 3.6). The executable has its
// own main (bench_main.cpp) and runs no test cases, but it links conjure::test_support, whose translation
// units include conjure/testing/doctest.h: every such unit references doctest's test-suite bookkeeping from
// a static initialiser (the header resets the current suite), so the symbols must exist in the binary.
// DOCTEST_CONFIG_IMPLEMENT defines them without a main; the configuration macros come from the shared
// header so the implementation matches every other unit.

#define DOCTEST_CONFIG_IMPLEMENT
#include "conjure/testing/doctest.h"
