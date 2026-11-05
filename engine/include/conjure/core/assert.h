#pragma once

#include "conjure/core/platform_detect.h"
#include "conjure/core/types.h"

#include <cstdlib>
#include <format>
#include <string>
#include <utility>

// Assertions.
//
//   CONJURE_ASSERT(index < size, "index {} out of range (size {})", index, size);
//   CONJURE_VERIFY(file.close());        // expression is still evaluated in Distribution
//   CONJURE_UNREACHABLE();
//
// CONJURE_ASSERT compiles to nothing in Distribution (the arguments stay in an unevaluated
// operand so they are still type-checked and never "unused"). CONJURE_VERIFY keeps evaluating its
// expression but drops the check. Failures go through a replaceable handler so tests can
// capture them instead of breaking into the debugger.
//
// Limitations: a condition containing an unparenthesised template comma must be wrapped in
// parentheses, and the macros cannot be used at namespace scope because they expand to a
// statement that names the enclosing function.

namespace conjure::core {

    /// <summary>
    /// Everything known about a failed assertion.
    /// </summary>
    struct assert_info {
        const char *expression = "";
        const char *file = "";
        int line = 0;
        const char *function = "";
        std::string message = {};
    };

    /// <summary>
    /// What the program does after the handler returns.
    /// </summary>
    enum class assert_action : u8 {
        BREAK,      // break into the debugger (or trap when none is attached)
        CONTINUE,   // carry on as if the assertion passed
        ABORT       // terminate the process
    };

    using assert_handler = assert_action (*)(const assert_info &info);

    /// <summary>
    /// Installs a new assertion handler and returns the previous one. Pass nullptr to restore
    /// the default handler, which logs the failure and breaks.
    /// </summary>
    assert_handler set_assert_handler(assert_handler handler) noexcept;

    /// <summary>
    /// The handler currently in effect.
    /// </summary>
    [[nodiscard]] assert_handler current_assert_handler() noexcept;

    /// <summary>
    /// The built-in handler: logs the failure and asks to break.
    /// </summary>
    assert_action default_assert_handler(const assert_info &info);

    namespace detail {
        /// <summary>
        /// Reports a failure to the active handler. Returns true when the caller should break.
        /// </summary>
        bool report_assert_failure(
          const char *expression,
          const char *file,
          int line,
          const char *function,
          std::string message
        );

        inline std::string format_assert_message() {
            return {};
        }

        template <typename... Args>
        std::string format_assert_message(std::format_string<Args...> format, Args &&...args) {
            return std::format(format, std::forward<Args>(args)...);
        }
    }
}

#define CONJURE_ASSERT_IMPL(condition, ...)                                                                 \
    do {                                                                                                    \
        if (!(condition)) [[unlikely]] {                                                                    \
            if (::conjure::core::detail::report_assert_failure(                                             \
                  #condition,                                                                               \
                  __FILE__,                                                                                 \
                  __LINE__,                                                                                 \
                  CONJURE_FUNCTION_NAME,                                                                    \
                  ::conjure::core::detail::format_assert_message(__VA_ARGS__)                               \
                )) {                                                                                        \
                CONJURE_DEBUG_BREAK();                                                                      \
            }                                                                                               \
        }                                                                                                   \
    } while (false)

#if CONJURE_ASSERTS_ENABLED
    #define CONJURE_ASSERT(condition, ...) CONJURE_ASSERT_IMPL(condition, __VA_ARGS__)
    #define CONJURE_VERIFY(condition, ...) CONJURE_ASSERT_IMPL(condition, __VA_ARGS__)
    // Reaching this is fatal even when the assertion handler continues: there is no valid
    // continuation and the Distribution build would treat it as undefined.
    #define CONJURE_UNREACHABLE()                                                                           \
        do {                                                                                                \
            CONJURE_ASSERT_IMPL(false, "unreachable code reached");                                         \
            std::abort();                                                                                   \
        } while (false)
#else
    #define CONJURE_ASSERT(condition, ...)                                                                  \
        static_cast<void>(sizeof(!(condition)) + sizeof(::conjure::core::detail::format_assert_message(__VA_ARGS__)))
    #define CONJURE_VERIFY(condition, ...)                                                                  \
        static_cast<void>(((condition), sizeof(::conjure::core::detail::format_assert_message(__VA_ARGS__))))
    #define CONJURE_UNREACHABLE() CONJURE_UNREACHABLE_HINT()
#endif

// Always-on check for conditions that must hold even in Distribution (e.g. memory corruption).
#define CONJURE_FATAL_ASSERT(condition, ...) CONJURE_ASSERT_IMPL(condition, __VA_ARGS__)
