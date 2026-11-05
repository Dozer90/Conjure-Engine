#include "conjure/testing/doctest.h"

#include "conjure/core/uuid.h"

#include <format>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

using namespace conjure;
using namespace conjure::core;

TEST_SUITE("core/uuid") {

    TEST_CASE("uuid - default uuid is null") {
        const uuid value = {};

        CHECK(value.is_null());
        CHECK(value == uuid::null());
        CHECK(value.hash() == 0);
        CHECK(value.to_string() == "00000000-0000-0000-0000-000000000000");
    }

    TEST_CASE("uuid - construction is constexpr") {
        constexpr uuid value(1, 2);
        static_assert(value.high() == 1);
        static_assert(value.low() == 2);
        static_assert(!value.is_null());
        static_assert(uuid::null().is_null());
        CHECK(true);
    }

    TEST_CASE("uuid - generated uuids are version 4 and unique") {
        std::unordered_set<uuid> seen = {};
        for (int index = 0; index < 10000; ++index) {
            const uuid value = uuid::generate();

            CHECK_FALSE(value.is_null());
            CHECK(((value.high() >> 12) & 0xF) == 4);
            CHECK(((value.low() >> 62) & 0x3) == 2);

            const bool inserted = seen.insert(value).second;
            CHECK(inserted);
        }
    }

    TEST_CASE("uuid - generation from several threads produces no duplicates") {
        constexpr int THREAD_COUNT = 8;
        constexpr int PER_THREAD = 2000;

        std::mutex mutex = {};
        std::vector<uuid> generated = {};
        generated.reserve(static_cast<usize>(THREAD_COUNT * PER_THREAD));

        std::vector<std::thread> threads = {};
        for (int threadIndex = 0; threadIndex < THREAD_COUNT; ++threadIndex) {
            threads.emplace_back([&mutex, &generated] {
                std::vector<uuid> local = {};
                local.reserve(PER_THREAD);
                for (int index = 0; index < PER_THREAD; ++index) {
                    local.push_back(uuid::generate());
                }
                const std::scoped_lock lock(mutex);
                generated.insert(generated.end(), local.begin(), local.end());
            });
        }
        for (std::thread &thread : threads) {
            thread.join();
        }

        const std::unordered_set<uuid> unique(generated.begin(), generated.end());
        CHECK(unique.size() == generated.size());
    }

    TEST_CASE("uuid - to_string and parse round trip") {
        const uuid original = uuid::generate();
        const std::string text = original.to_string();

        REQUIRE(text.size() == 36);
        const std::optional<uuid> parsed = uuid::parse(text);
        REQUIRE(parsed.has_value());
        CHECK(*parsed == original);

        const uuid allSet(~0ull, ~0ull);
        CHECK(allSet.to_string() == "ffffffff-ffff-ffff-ffff-ffffffffffff");
        CHECK(uuid::parse(allSet.to_string()) == allSet);
    }

    TEST_CASE("uuid - parse accepts known values, braces and upper case") {
        const std::optional<uuid> lower = uuid::parse("123e4567-e89b-12d3-a456-426614174000");
        REQUIRE(lower.has_value());
        CHECK(lower->high() == 0x123e4567e89b12d3ull);
        CHECK(lower->low() == 0xa456426614174000ull);

        const std::optional<uuid> upper = uuid::parse("123E4567-E89B-12D3-A456-426614174000");
        REQUIRE(upper.has_value());
        CHECK(*upper == *lower);

        const std::optional<uuid> braced = uuid::parse("{123e4567-e89b-12d3-a456-426614174000}");
        REQUIRE(braced.has_value());
        CHECK(*braced == *lower);

        const std::optional<uuid> bracedUpper = uuid::parse("{123E4567-E89B-12D3-A456-426614174000}");
        REQUIRE(bracedUpper.has_value());
        CHECK(*bracedUpper == *lower);

        const std::optional<uuid> null = uuid::parse("00000000-0000-0000-0000-000000000000");
        REQUIRE(null.has_value());
        CHECK(null->is_null());
    }

    TEST_CASE("uuid - parse rejects malformed input") {
        CHECK_FALSE(uuid::parse("").has_value());
        CHECK_FALSE(uuid::parse("123e4567e89b12d3a456426614174000").has_value());
        CHECK_FALSE(uuid::parse("123e4567-e89b-12d3-a456-42661417400").has_value());
        CHECK_FALSE(uuid::parse("123e4567-e89b-12d3-a456-4266141740000").has_value());
        CHECK_FALSE(uuid::parse("123e4567_e89b-12d3-a456-426614174000").has_value());
        CHECK_FALSE(uuid::parse("123e4567-e89b-12d3-a456-42661417400g").has_value());
        CHECK_FALSE(uuid::parse("{123e4567-e89b-12d3-a456-426614174000").has_value());
        CHECK_FALSE(uuid::parse("123e4567-e89b-12d3-a456-426614174000}").has_value());
        CHECK_FALSE(uuid::parse("{}").has_value());
    }

    TEST_CASE("uuid - uuids order and hash consistently") {
        const uuid smaller(1, 0);
        const uuid larger(1, 1);

        CHECK(smaller < larger);
        CHECK(uuid(0, ~0ull) < uuid(1, 0));
        CHECK(std::hash<uuid>{}(smaller) == smaller.hash());
        CHECK(smaller.hash() != larger.hash());
        CHECK(uuid(1, 0).hash() != uuid(0, 1).hash());
    }

    TEST_CASE("uuid - uuid is formattable, including with format specs") {
        const uuid value(0x123e4567e89b12d3ull, 0xa456426614174000ull);

        CHECK(std::format("{}", value) == "123e4567-e89b-12d3-a456-426614174000");
        CHECK(std::format("[{:>38}]", uuid::null()) == "[  00000000-0000-0000-0000-000000000000]");
    }
}
