// Tiny shared library loaded by the core::dynamic_library tests.

#include "conjure/core/platform_detect.h"

extern "C" {
    CONJURE_SYMBOL_EXPORT int conjure_test_add(int left, int right) {
        return left + right;
    }

    CONJURE_SYMBOL_EXPORT int conjure_test_constant = 42;
}
