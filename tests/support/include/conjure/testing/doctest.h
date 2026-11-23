#pragma once

// The only place doctest is included. Tests never include <doctest/doctest.h> directly: every test
// file opens with this header so the doctest configuration is identical in every translation unit,
// including the one in test_main.cpp that defines DOCTEST_CONFIG_IMPLEMENT.
//
//   #include "conjure/testing/doctest.h"
//
//   TEST_SUITE("core/slot_map") {
//       TEST_CASE("slot_map - erase invalidates the handle generation") { ... }
//   }

#include "conjure/testing/test_environment.h"

#include "conjure/core/error.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"

#define DOCTEST_CONFIG_SUPER_FAST_ASSERTS
#define DOCTEST_CONFIG_USE_STD_HEADERS
#include <doctest/doctest.h>

#include <string>

namespace conjure::testing {

    /// <summary>
    /// "ok" for a successful result, otherwise the error's "category.CODE: message" text. Used by
    /// the result macros below to explain a failure.
    /// </summary>
    template <typename ValueType>
    [[nodiscard]] std::string describe_error(const core::result<ValueType> &value) {
        if (value.has_value()) {
            return "ok";
        }
        return value.get_error().to_string();
    }

    /// <summary>
    /// True when the result failed with exactly this code of its category.
    /// </summary>
    template <typename ValueType, core::error_enum EnumType>
    [[nodiscard]] bool has_error(const core::result<ValueType> &value, EnumType code) noexcept {
        return !value.has_value() && value.get_error().is(code);
    }
}

// Result helpers. CONJURE_REQUIRE_OK stops the case; CONJURE_CHECK_OK continues;
// CONJURE_CHECK_ERROR(expression, errorCode) checks for one specific error code. The expression is
// evaluated exactly once.
#define CONJURE_REQUIRE_OK(expression)                                                                      \
    do {                                                                                                    \
        auto &&CONJURE_UNIQUE_NAME(conjureResult_) = (expression);                                          \
        REQUIRE_MESSAGE(                                                                                    \
          CONJURE_UNIQUE_NAME(conjureResult_).has_value(),                                                  \
          ::conjure::testing::describe_error(CONJURE_UNIQUE_NAME(conjureResult_))                           \
        );                                                                                                  \
    } while (false)

#define CONJURE_CHECK_OK(expression)                                                                        \
    do {                                                                                                    \
        auto &&CONJURE_UNIQUE_NAME(conjureResult_) = (expression);                                          \
        CHECK_MESSAGE(                                                                                      \
          CONJURE_UNIQUE_NAME(conjureResult_).has_value(),                                                  \
          ::conjure::testing::describe_error(CONJURE_UNIQUE_NAME(conjureResult_))                           \
        );                                                                                                  \
    } while (false)

#define CONJURE_CHECK_ERROR(expression, errorCode)                                                          \
    do {                                                                                                    \
        auto &&CONJURE_UNIQUE_NAME(conjureResult_) = (expression);                                          \
        CHECK_MESSAGE(                                                                                      \
          ::conjure::testing::has_error(CONJURE_UNIQUE_NAME(conjureResult_), (errorCode)),                  \
          "expected error " << #errorCode << ", got "                                                       \
            << ::conjure::testing::describe_error(CONJURE_UNIQUE_NAME(conjureResult_))                      \
        );                                                                                                  \
    } while (false)

// Precondition guard; the first statement of a case that needs something from the machine
// (docs/design/08_testing.md 3.2). Returns from the case in a direct run, exits 77 under CTest.
#define CONJURE_TEST_REQUIRE(preconditionValue)                                                             \
    if (!::conjure::testing::require_precondition(                                                          \
          (preconditionValue),                                                                              \
          __FILE__,                                                                                         \
          static_cast<::conjure::u32>(__LINE__)                                                             \
        )) {                                                                                                \
        return;                                                                                             \
    }                                                                                                       \
    static_assert(true, "")

// Deprecated-call markers, for the case that pins a forwarder kept for one wave under a renamed function's
// former name (code sweep F624: job_handle::valid() forwards to is_valid()). The warning for using a
// [[deprecated]] function is silenced between the two markers only and stays an error everywhere else. Each
// marker expands to pragmas only, so the pair goes around whole statements inside a function body:
//
//   CONJURE_TEST_DEPRECATED_BEGIN
//   const bool former = handle.valid();
//   CONJURE_TEST_DEPRECATED_END
//   CHECK(former == handle.is_valid());
#if CONJURE_COMPILER_MSVC
    #define CONJURE_TEST_DEPRECATED_BEGIN __pragma(warning(push)) __pragma(warning(disable : 4996))
    #define CONJURE_TEST_DEPRECATED_END __pragma(warning(pop))
#elif CONJURE_COMPILER_CLANG
    #define CONJURE_TEST_DEPRECATED_BEGIN                                                                   \
        _Pragma("clang diagnostic push")                                                                    \
        _Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"")
    #define CONJURE_TEST_DEPRECATED_END _Pragma("clang diagnostic pop")
#else
    #define CONJURE_TEST_DEPRECATED_BEGIN                                                                   \
        _Pragma("GCC diagnostic push")                                                                      \
        _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
    #define CONJURE_TEST_DEPRECATED_END _Pragma("GCC diagnostic pop")
#endif

// Readable values in failed CHECKs for the foundation types every module uses. Math types are in
// approx.h.
namespace doctest {

    template <>
    struct StringMaker<conjure::core::uuid> {
        static String convert(const conjure::core::uuid &value) {
            return String(value.to_string().c_str());
        }
    };

    template <>
    struct StringMaker<conjure::core::error> {
        static String convert(const conjure::core::error &value) {
            return String(value.to_string().c_str());
        }
    };

    // doctest prints enumerations through their underlying integer unless a toString overload exists.
    inline String toString(conjure::core::error_code value) {
        const std::string_view name = conjure::core::to_string(value);
        return String(name.data(), static_cast<String::size_type>(name.size()));
    }
}
