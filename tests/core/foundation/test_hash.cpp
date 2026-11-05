#include "conjure/testing/doctest.h"

#include "conjure/core/hash.h"

#include <array>
#include <cstring>
#include <span>
#include <string_view>

using namespace conjure;
using namespace conjure::core;

TEST_SUITE("core/hash") {

    // Published FNV-1a test vectors. Conjure.Managed's NameHash tests use the same table.
    TEST_CASE("hash - fnv1a64 matches the reference vectors") {
        CHECK(hash::fnv1a64("") == 0xCBF29CE484222325ull);
        CHECK(hash::fnv1a64("a") == 0xAF63DC4C8601EC8Cull);
        CHECK(hash::fnv1a64("foobar") == 0x85944171F73967E8ull);
        CHECK(hash::fnv1a64("hello world") == 0x779A65E7023CD2E7ull);
    }

    TEST_CASE("hash - fnv1a32 matches the reference vectors") {
        CHECK(hash::fnv1a32("") == 0x811C9DC5u);
        CHECK(hash::fnv1a32("a") == 0xE40C292Cu);
        CHECK(hash::fnv1a32("foobar") == 0xBF9CF968u);
    }

    TEST_CASE("hash - string and byte overloads agree") {
        constexpr std::string_view SAMPLE = "conjure";
        std::array<std::byte, 7> bytes = {};
        std::memcpy(bytes.data(), SAMPLE.data(), SAMPLE.size());

        CHECK(hash::fnv1a64(SAMPLE) == hash::fnv1a64(std::span<const std::byte>(bytes)));
        CHECK(hash::fnv1a32(SAMPLE) == hash::fnv1a32(std::span<const std::byte>(bytes)));
    }

    TEST_CASE("hash - hashes are usable in constant expressions") {
        constexpr u64 wide = hash::fnv1a64("constexpr");
        constexpr u32 narrow = hash::fnv1a32("constexpr");
        static_assert(wide != 0);
        static_assert(narrow != 0);
        static_assert(hash::fnv1a64("a") != hash::fnv1a64("b"));
        CHECK(true);
    }

    TEST_CASE("hash - seeding continues a hash over concatenated input") {
        const u64 whole = hash::fnv1a64("hello world");
        const u64 chained = hash::fnv1a64(" world", hash::fnv1a64("hello"));
        CHECK(whole == chained);

        const u32 whole32 = hash::fnv1a32("hello world");
        const u32 chained32 = hash::fnv1a32(" world", hash::fnv1a32("hello"));
        CHECK(whole32 == chained32);
    }

    TEST_CASE("hash - combine is order dependent and non-trivial") {
        const u64 first = hash::combine(hash::fnv1a64("a"), hash::fnv1a64("b"));
        const u64 second = hash::combine(hash::fnv1a64("b"), hash::fnv1a64("a"));

        CHECK(first != second);
        CHECK(hash::combine(0, 0) != 0);
        CHECK(hash::combine(1, 2) != hash::combine(1, 3));
    }
}
