// Shared doctest entry point for every Conjure test executable.
//
// Unexpected assertion failures are turned into doctest failures (instead of breaking into the
// debugger) so a bad assert fails the test run in CI rather than hanging or aborting it.

#include "conjure/core/assert.h"
#include "conjure/core/log.h"

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

namespace {
    conjure::core::assert_action test_assert_handler(const conjure::core::assert_info &info) {
        ADD_FAIL_CHECK_AT(
          info.file,
          info.line,
          "Unexpected assertion failure: " << info.expression << " - " << info.message
        );
        return conjure::core::assert_action::CONTINUE;
    }
}

int main(int argc, char **argv) {
    conjure::core::log::set_console_enabled(false);
    conjure::core::set_assert_handler(&test_assert_handler);

    doctest::Context context(argc, argv);

    const int result = context.run();

    conjure::core::log::flush();
    return result;
}
