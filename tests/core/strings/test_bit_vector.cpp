#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"

#include "conjure/core/bit_vector.h"

#include <iterator>
#include <ranges>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;

namespace {
    std::vector<usize> collect_set_bits(const bit_vector &bits) {
        std::vector<usize> indices = {};
        for (const usize index : bits.set_bits()) {
            indices.push_back(index);
        }
        return indices;
    }
}

TEST_SUITE("core/bit_vector") {

    TEST_CASE("bit_vector - default constructed vector is empty") {
        const bit_vector bits = {};

        CHECK(bits.size() == 0);
        CHECK(bits.empty());
        CHECK(bits.word_count() == 0);
        CHECK(bits.count() == 0);
        CHECK(bits.none());
        CHECK_FALSE(bits.any());
        CHECK(bits.all());
        CHECK(bits.find_first_set() == bit_vector::NPOS);
        CHECK(bits.find_first_clear() == bit_vector::NPOS);
        CHECK(bits.find_last_set() == bit_vector::NPOS);
        CHECK(bits.words().empty());
    }

    TEST_CASE("bit_vector - sized construction fills with the requested value and keeps padding bits clear") {
        const bit_vector clear(70);
        CHECK(clear.size() == 70);
        CHECK(clear.word_count() == 2);
        CHECK(clear.none());
        CHECK(clear.count() == 0);

        const bit_vector set(70, true);
        CHECK(set.size() == 70);
        CHECK(set.all());
        CHECK(set.count() == 70);
        REQUIRE(set.words().size() == 2);
        CHECK(set.words()[0] == ~u64{0});
        CHECK(set.words()[1] == 0x3Full);

        const bit_vector exact(128, true);
        CHECK(exact.word_count() == 2);
        CHECK(exact.count() == 128);
        CHECK(exact.all());
    }

    TEST_CASE("bit_vector - set, reset, flip and test address individual bits across word boundaries") {
        bit_vector bits(130);

        bits.set(0);
        bits.set(63);
        bits.set(64);
        bits.set(129);
        CHECK(bits.test(0));
        CHECK(bits.test(63));
        CHECK(bits.test(64));
        CHECK(bits.test(129));
        CHECK_FALSE(bits.test(1));
        CHECK_FALSE(bits.test(65));
        CHECK(bits[129]);
        CHECK(bits.count() == 4);

        bits.reset(63);
        CHECK_FALSE(bits.test(63));
        CHECK(bits.count() == 3);

        bits.flip(63);
        bits.flip(0);
        CHECK(bits.test(63));
        CHECK_FALSE(bits.test(0));

        bits.set(5, true);
        bits.set(64, false);
        CHECK(bits.test(5));
        CHECK_FALSE(bits.test(64));
    }

    TEST_CASE("bit_vector - out-of-range access asserts and is otherwise harmless") {
        testing::expect_assert asserts = {};
        bit_vector bits(10);

        CHECK_FALSE(bits.test(10));
        CHECK(asserts.count() == 1);

        bits.set(10);
        bits.reset(64);
        bits.flip(1000);
        bits.set(10, true);
        CHECK(asserts.count() == 5);
        CHECK(bits.none());
        CHECK(bits.size() == 10);
        CHECK(bits.words()[0] == 0);

        const bit_vector empty = {};
        CHECK_FALSE(empty.test(0));
        CHECK(asserts.count() == 6);
    }

    TEST_CASE("bit_vector - resize grows with the requested value and shrinks dropping bits") {
        bit_vector bits(10, true);

        bits.resize(70, false);
        CHECK(bits.size() == 70);
        CHECK(bits.count() == 10);
        CHECK(bits.test(9));
        CHECK_FALSE(bits.test(10));
        CHECK_FALSE(bits.test(69));

        bits.resize(100, true);
        CHECK(bits.size() == 100);
        CHECK(bits.count() == 40);
        CHECK_FALSE(bits.test(69));
        CHECK(bits.test(70));
        CHECK(bits.test(99));

        bits.resize(5);
        CHECK(bits.size() == 5);
        CHECK(bits.word_count() == 1);
        CHECK(bits.count() == 5);
        CHECK(bits.words()[0] == 0x1Full);

        bits.resize(5);
        CHECK(bits.size() == 5);

        bits.resize(0);
        CHECK(bits.empty());
        CHECK(bits.word_count() == 0);
    }

    TEST_CASE("bit_vector - growing with true from a partially filled word sets exactly the new bits") {
        bit_vector bits(3);
        bits.set(1);

        bits.resize(64, true);
        CHECK(bits.count() == 62);
        CHECK_FALSE(bits.test(0));
        CHECK(bits.test(1));
        CHECK_FALSE(bits.test(2));
        CHECK(bits.test(3));
        CHECK(bits.test(63));

        bits.resize(65, true);
        CHECK(bits.test(64));
        CHECK(bits.count() == 63);
        CHECK(bits.words()[1] == 1);
    }

    TEST_CASE("bit_vector - push_back appends bits one at a time") {
        bit_vector bits = {};
        bits.push_back(true);
        bits.push_back(false);
        bits.push_back(true);

        CHECK(bits.size() == 3);
        CHECK(bits.test(0));
        CHECK_FALSE(bits.test(1));
        CHECK(bits.test(2));
        CHECK(bits.count() == 2);
    }

    TEST_CASE("bit_vector - clear empties the vector and reserve grows capacity without changing size") {
        bit_vector bits(100, true);
        bits.clear();
        CHECK(bits.empty());
        CHECK(bits.word_count() == 0);

        bits.reserve(1000);
        CHECK(bits.size() == 0);
        CHECK(bits.capacity() >= 1000);
        CHECK(bits.capacity() % bit_vector::WORD_BITS == 0);
    }

    TEST_CASE("bit_vector - set_all, reset_all and flip_all affect every real bit and never the padding") {
        bit_vector bits(70);

        bits.set_all();
        CHECK(bits.all());
        CHECK(bits.count() == 70);
        CHECK(bits.words()[1] == 0x3Full);

        bits.flip_all();
        CHECK(bits.none());
        CHECK(bits.words()[1] == 0);

        bits.set(3);
        bits.flip_all();
        CHECK(bits.count() == 69);
        CHECK_FALSE(bits.test(3));
        CHECK(bits.words()[1] == 0x3Full);

        bits.reset_all();
        CHECK(bits.none());
        CHECK(bits.size() == 70);
    }

    TEST_CASE("bit_vector - any, none, all and count report the state of the whole vector") {
        bit_vector bits(200);
        CHECK(bits.none());
        CHECK_FALSE(bits.any());
        CHECK_FALSE(bits.all());
        CHECK(bits.count() == 0);

        bits.set(150);
        CHECK(bits.any());
        CHECK_FALSE(bits.none());
        CHECK_FALSE(bits.all());
        CHECK(bits.count() == 1);

        bits.set_all();
        bits.reset(199);
        CHECK_FALSE(bits.all());
        CHECK(bits.count() == 199);

        bits.set(199);
        CHECK(bits.all());

        const bit_vector fullWords(64, true);
        CHECK(fullWords.all());
        bit_vector almost(64, true);
        almost.reset(63);
        CHECK_FALSE(almost.all());
    }

    TEST_CASE("bit_vector - find_first_set walks set bits across words and stops at the end") {
        bit_vector bits(300);
        bits.set(2);
        bits.set(63);
        bits.set(64);
        bits.set(200);
        bits.set(299);

        CHECK(bits.find_first_set() == 2);
        CHECK(bits.find_first_set(2) == 2);
        CHECK(bits.find_first_set(3) == 63);
        CHECK(bits.find_first_set(64) == 64);
        CHECK(bits.find_first_set(65) == 200);
        CHECK(bits.find_first_set(201) == 299);
        CHECK(bits.find_first_set(299) == 299);
        CHECK(bits.find_first_set(300) == bit_vector::NPOS);
        CHECK(bits.find_first_set(100000) == bit_vector::NPOS);
        CHECK(bits.find_last_set() == 299);

        const bit_vector none(300);
        CHECK(none.find_first_set() == bit_vector::NPOS);
        CHECK(none.find_last_set() == bit_vector::NPOS);
    }

    TEST_CASE("bit_vector - find_first_clear skips set bits and ignores padding past the end") {
        bit_vector bits(70, true);
        CHECK(bits.find_first_clear() == bit_vector::NPOS);

        bits.reset(64);
        CHECK(bits.find_first_clear() == 64);
        CHECK(bits.find_first_clear(64) == 64);
        CHECK(bits.find_first_clear(65) == bit_vector::NPOS);

        bits.reset(0);
        CHECK(bits.find_first_clear() == 0);
        CHECK(bits.find_first_clear(1) == 64);

        bit_vector exact(128, true);
        CHECK(exact.find_first_clear() == bit_vector::NPOS);
        exact.reset(127);
        CHECK(exact.find_first_clear(100) == 127);
    }

    TEST_CASE("bit_vector - set_bits iterates the indices of set bits in ascending order") {
        bit_vector bits(140);
        CHECK(collect_set_bits(bits).empty());

        bits.set(0);
        bits.set(7);
        bits.set(63);
        bits.set(64);
        bits.set(139);
        CHECK(collect_set_bits(bits) == std::vector<usize>{0, 7, 63, 64, 139});

        bit_vector::set_bit_iterator iterator = bits.set_bits().begin();
        CHECK(*iterator == 0);
        const bit_vector::set_bit_iterator copy = iterator++;
        CHECK(*copy == 0);
        CHECK(*iterator == 7);
        CHECK(iterator != bits.set_bits().end());

        const bit_vector full(3, true);
        CHECK(collect_set_bits(full) == std::vector<usize>{0, 1, 2});
    }

    TEST_CASE("bit_vector - set_bit_iterator models std::forward_iterator and works with ranges algorithms") {
        static_assert(std::forward_iterator<bit_vector::set_bit_iterator>);
        static_assert(std::sentinel_for<bit_vector::set_bit_iterator, bit_vector::set_bit_iterator>);
        static_assert(std::ranges::forward_range<bit_vector::set_bit_range>);
        static_assert(std::ranges::borrowed_range<bit_vector::set_bit_range>);
        static_assert(std::same_as<std::iter_value_t<bit_vector::set_bit_iterator>, usize>);
        // Dereferencing yields a prvalue, so the legacy category must not promise a forward iterator.
        static_assert(std::same_as<
          std::iterator_traits<bit_vector::set_bit_iterator>::iterator_category,
          std::input_iterator_tag
        >);

        bit_vector bits(130);
        CHECK(std::ranges::distance(bits.set_bits()) == 0);
        bits.set(3);
        bits.set(64);
        bits.set(129);
        CHECK(std::ranges::distance(bits.set_bits()) == 3);
        CHECK(std::ranges::distance(bits.set_bits()) == static_cast<isize>(bits.count()));

        const auto found = std::ranges::find(bits.set_bits(), usize{64});
        REQUIRE(found != bits.set_bits().end());
        CHECK(*found == 64);
        CHECK(std::ranges::find(bits.set_bits(), usize{65}) == bits.set_bits().end());

        std::vector<usize> copied = {};
        std::ranges::copy(bits.set_bits(), std::back_inserter(copied));
        CHECK(copied == std::vector<usize>{3, 64, 129});
    }

    TEST_CASE("bit_vector - incrementing the end or a default-constructed iterator asserts and stays at the end") {
        testing::expect_assert asserts = {};
        bit_vector bits(10);
        bits.set(0);
        bits.set(9);

        bit_vector::set_bit_iterator iterator = bits.set_bits().begin();
        ++iterator;
        ++iterator;
        REQUIRE(iterator == bits.set_bits().end());
        CHECK(asserts.count() == 0);

        // NPOS + 1 would wrap to 0 and silently restart the iteration from the first set bit.
        ++iterator;
        CHECK(asserts.count() == 1);
        CHECK(iterator == bits.set_bits().end());
        iterator++;
        CHECK(asserts.count() == 2);
        CHECK(iterator == bits.set_bits().end());

        bit_vector::set_bit_iterator detached = {};
        CHECK(detached == bits.set_bits().end());
        ++detached;
        CHECK(asserts.count() == 3);
        CHECK(detached == bits.set_bits().end());
    }

    TEST_CASE("bit_vector - bitwise operators combine vectors of equal size") {
        bit_vector left(70);
        bit_vector right(70);
        left.set(1);
        left.set(65);
        right.set(1);
        right.set(66);

        const bit_vector both = left & right;
        CHECK(collect_set_bits(both) == std::vector<usize>{1});

        const bit_vector either = left | right;
        CHECK(collect_set_bits(either) == std::vector<usize>{1, 65, 66});

        const bit_vector exclusive = left ^ right;
        CHECK(collect_set_bits(exclusive) == std::vector<usize>{65, 66});

        const bit_vector inverted = ~left;
        CHECK(inverted.count() == 68);
        CHECK_FALSE(inverted.test(1));
        CHECK(inverted.test(0));
        CHECK(inverted.words()[1] == (0x3Full & ~(u64{1} << 1)));

        bit_vector compound = left;
        compound |= right;
        CHECK(compound == either);
        compound &= right;
        CHECK(compound == right);
        compound ^= right;
        CHECK(compound.none());
        CHECK(compound.size() == 70);
    }

    TEST_CASE("bit_vector - remove, is_subset_of and intersects") {
        bit_vector mask(10);
        mask.set(1);
        mask.set(2);
        mask.set(3);
        bit_vector other(10);
        other.set(2);

        CHECK(other.is_subset_of(mask));
        CHECK_FALSE(mask.is_subset_of(other));
        CHECK(mask.intersects(other));

        mask.remove(other);
        CHECK(collect_set_bits(mask) == std::vector<usize>{1, 3});
        CHECK_FALSE(mask.intersects(other));
        CHECK_FALSE(other.is_subset_of(mask));

        const bit_vector empty(10);
        CHECK(empty.is_subset_of(mask));
        CHECK_FALSE(empty.intersects(mask));
    }

    TEST_CASE("bit_vector - combining vectors of different sizes asserts") {
        testing::expect_assert asserts = {};
        // Not "small": that is a Windows rpcndr.h macro.
        bit_vector fewBits(10);
        bit_vector manyBits(100);
        fewBits.set(1);
        manyBits.set(1);
        manyBits.set(90);

        fewBits &= manyBits;
        fewBits |= manyBits;
        fewBits ^= manyBits;
        fewBits.remove(manyBits);
        CHECK(asserts.count() == 4);
        CHECK(fewBits.size() == 10);

        CHECK(fewBits.is_subset_of(manyBits));
        CHECK_FALSE(fewBits.intersects(manyBits));
        CHECK(asserts.count() == 6);
    }

    TEST_CASE("bit_vector - equality compares size and every bit") {
        bit_vector first(70);
        bit_vector second(70);
        CHECK(first == second);

        first.set(69);
        CHECK_FALSE(first == second);
        second.set(69);
        CHECK(first == second);

        const bit_vector shorter(69);
        CHECK_FALSE(first == shorter);
        const bit_vector empty = {};
        CHECK(empty == bit_vector());
    }

    TEST_CASE("bit_vector - copy and move preserve contents") {
        bit_vector original(130);
        original.set(0);
        original.set(129);

        const bit_vector copy = original;
        CHECK(copy == original);

        bit_vector moved = std::move(original);
        CHECK(moved == copy);
        CHECK(moved.size() == 130);
        CHECK(moved.test(129));

        bit_vector assigned = {};
        assigned = copy;
        CHECK(assigned == copy);
        bit_vector moveAssigned = {};
        moveAssigned = std::move(moved);
        CHECK(moveAssigned == copy);
    }

    TEST_CASE("bit_vector - a moved-from vector is empty and reusable") {
        bit_vector source(100);
        source.set(3);

        SUBCASE("move construction") {
            const bit_vector destination = std::move(source);
            CHECK(destination.size() == 100);
            CHECK(destination.test(3));
        }

        SUBCASE("move assignment into an existing vector") {
            bit_vector destination(130, true);
            destination = std::move(source);
            CHECK(destination.size() == 100);
            CHECK(destination.count() == 1);
            CHECK(destination.test(3));
        }

        // A defaulted move kept the old size over an emptied word array, so set() and test() indexed past it.
        REQUIRE(source.size() == 0);
        CHECK(source.empty());
        CHECK(source.word_count() == 0);
        CHECK(source == bit_vector());

        source.resize(100);
        CHECK(source.word_count() == 2);
        CHECK(source.none());
        source.set(3);
        CHECK(source.test(3));
        CHECK(source.count() == 1);
    }

    TEST_CASE("bit_vector - self move assignment keeps the bits") {
        bit_vector bits(70);
        bits.set(69);

        // Self-assignment through an alias (a literal self-move is a compiler warning) is a no-op.
        bit_vector &alias = bits;
        bits = std::move(alias);
        CHECK(bits.size() == 70);
        CHECK(bits.word_count() == 2);
        CHECK(bits.test(69));
        CHECK(bits.count() == 1);
    }

    TEST_CASE("bit_vector - large vectors behave at word multiples and beyond") {
        constexpr usize SIZE = 64 * 1024 + 1;
        bit_vector bits(SIZE);
        for (usize index = 0; index < SIZE; index += 1000) {
            bits.set(index);
        }

        CHECK(bits.count() == (SIZE + 999) / 1000);
        CHECK(bits.find_last_set() == 65000);
        usize visited = 0;
        usize previous = 0;
        for (const usize index : bits.set_bits()) {
            CHECK(index % 1000 == 0);
            CHECK((visited == 0 || index > previous));
            previous = index;
            ++visited;
        }
        CHECK(visited == bits.count());
        CHECK(bits.find_first_clear(65000) == 65001);
        CHECK(bits.find_first_clear(SIZE - 1) == SIZE - 1);
        bits.set(SIZE - 1);
        CHECK(bits.find_first_clear(SIZE - 1) == bit_vector::NPOS);
        CHECK(bits.find_last_set() == SIZE - 1);
    }
}
