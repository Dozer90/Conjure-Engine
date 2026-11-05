#include "conjure/testing/doctest.h"

#include "conjure/core/enum_flags.h"
#include "conjure/core/types.h"

using namespace conjure;

namespace {
    enum class test_flags : u8 {
        NONE = 0,
        FIRST = 1 << 0,
        SECOND = 1 << 1,
        THIRD = 1 << 2,
        ALL = FIRST | SECOND | THIRD
    };
    CONJURE_ENUM_FLAGS(test_flags);

    enum class wide_flags : u32 {
        NONE = 0,
        LOW = 1u << 0,
        TOP = 1u << 31
    };
    CONJURE_ENUM_FLAGS(wide_flags);

    enum class huge_flags : u64 {
        NONE = 0,
        TOP = 1ull << 63
    };
    CONJURE_ENUM_FLAGS(huge_flags);
}

TEST_SUITE("core/enum_flags") {

    TEST_CASE("enum_flags - bitwise operators combine flags") {
        const test_flags combined = test_flags::FIRST | test_flags::THIRD;

        CHECK(to_underlying(combined) == 5);
        CHECK((combined & test_flags::FIRST) == test_flags::FIRST);
        CHECK((combined & test_flags::SECOND) == test_flags::NONE);
        CHECK((combined ^ test_flags::FIRST) == test_flags::THIRD);
        CHECK((~test_flags::NONE & test_flags::ALL) == test_flags::ALL);
    }

    TEST_CASE("enum_flags - compound assignment operators modify in place") {
        test_flags flags = test_flags::NONE;

        flags |= test_flags::SECOND;
        CHECK(flags == test_flags::SECOND);

        flags |= test_flags::THIRD;
        flags &= test_flags::THIRD;
        CHECK(flags == test_flags::THIRD);

        flags ^= test_flags::ALL;
        CHECK(flags == (test_flags::FIRST | test_flags::SECOND));
    }

    TEST_CASE("enum_flags - has_flag requires every bit, has_any_flag requires one") {
        const test_flags flags = test_flags::FIRST | test_flags::SECOND;

        CHECK(has_flag(flags, test_flags::FIRST));
        CHECK(has_flag(flags, test_flags::FIRST | test_flags::SECOND));
        CHECK_FALSE(has_flag(flags, test_flags::ALL));
        CHECK(has_any_flag(flags, test_flags::ALL));
        CHECK_FALSE(has_any_flag(flags, test_flags::THIRD));
    }

    TEST_CASE("enum_flags - NONE is always contained and never matched by has_any_flag") {
        CHECK(has_flag(test_flags::NONE, test_flags::NONE));
        CHECK(has_flag(test_flags::ALL, test_flags::NONE));
        CHECK_FALSE(has_any_flag(test_flags::ALL, test_flags::NONE));
        CHECK_FALSE(has_any_flag(test_flags::NONE, test_flags::NONE));
    }

    TEST_CASE("enum_flags - with_flag and without_flag return modified copies") {
        const test_flags flags = test_flags::FIRST;

        CHECK(with_flag(flags, test_flags::THIRD) == (test_flags::FIRST | test_flags::THIRD));
        CHECK(with_flag(flags, test_flags::FIRST) == test_flags::FIRST);
        CHECK(without_flag(test_flags::ALL, test_flags::SECOND) == (test_flags::FIRST | test_flags::THIRD));
        CHECK(without_flag(flags, test_flags::SECOND) == test_flags::FIRST);
        CHECK(flags == test_flags::FIRST);
    }

    TEST_CASE("enum_flags - wide underlying types keep their top bit") {
        const wide_flags both = wide_flags::LOW | wide_flags::TOP;
        CHECK(to_underlying(both) == 0x80000001u);
        CHECK(has_flag(both, wide_flags::TOP));
        CHECK(to_underlying(~wide_flags::TOP) == 0x7FFFFFFFu);

        CHECK(to_underlying(huge_flags::TOP | huge_flags::NONE) == (1ull << 63));
        CHECK(to_underlying(~huge_flags::NONE) == ~0ull);
        CHECK(without_flag(huge_flags::TOP, huge_flags::TOP) == huge_flags::NONE);
    }

    TEST_CASE("enum_flags - operators are constexpr") {
        constexpr test_flags combined = test_flags::FIRST | test_flags::SECOND;
        static_assert(has_flag(combined, test_flags::SECOND));
        static_assert(!has_flag(combined, test_flags::THIRD));

        constexpr auto compound = [] {
            test_flags flags = test_flags::NONE;
            flags |= test_flags::THIRD;
            flags ^= test_flags::ALL;
            flags &= ~test_flags::FIRST;
            return flags;
        }();
        static_assert(compound == test_flags::SECOND);
        CHECK(true);
    }
}
