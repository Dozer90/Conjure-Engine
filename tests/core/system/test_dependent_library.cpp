// Tiny shared library that depends on conjure_test_shared_library, loaded by the
// core::dynamic_library tests to exercise dependency resolution.

#include "conjure/core/platform_detect.h"

extern "C" int conjure_test_add(int left, int right);

extern "C" {
    CONJURE_SYMBOL_EXPORT int conjure_test_add_twice(int left, int right) {
        return conjure_test_add(left, right) * 2;
    }
}
