#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"

#include "tracked_value.h"

#include "conjure/core/containers/stable_vector.h"

#include <algorithm>
#include <compare>
#include <iterator>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;

namespace {
    struct no_default {
        int value = 0;

        explicit no_default(int initial) :
            value(initial) {}
    };

    template <typename Vector>
    std::vector<int> values_of(const Vector &container) {
        std::vector<int> result = {};
        for (const auto &element : container) {
            if constexpr (std::is_same_v<std::remove_cvref_t<decltype(element)>, testing::tracked_value>) {
                result.push_back(element.value());
            } else {
                result.push_back(element);
            }
        }
        return result;
    }
}

TEST_SUITE("core/stable_vector") {

    TEST_CASE("stable_vector - default constructed is empty with no chunks") {
        stable_vector<int> vector = {};
        CHECK(vector.empty());
        CHECK(vector.size() == 0);
        CHECK(vector.capacity() == 0);
        CHECK(vector.chunk_count() == 0);
        CHECK(vector.begin() == vector.end());
        CHECK(stable_vector<int>::CHUNK_SIZE == 64);
        CHECK(stable_vector<int, 16>::CHUNK_SIZE == 16);
    }

    TEST_CASE("stable_vector - emplace_back returns references that never move") {
        stable_vector<std::string, 4> vector = {};
        std::vector<std::string *> addresses = {};
        for (int value = 0; value < 100; ++value) {
            std::string &element = vector.emplace_back(std::to_string(value));
            CHECK(element == std::to_string(value));
            addresses.push_back(&element);
        }
        CHECK(vector.size() == 100);
        CHECK(vector.chunk_count() == 25);
        CHECK(vector.capacity() == 100);

        for (usize index = 0; index < addresses.size(); ++index) {
            CAPTURE(index);
            CHECK(&vector[index] == addresses[index]);
            CHECK(*addresses[index] == std::to_string(index));
        }

        std::string &pushed = vector.push_back("copied");
        CHECK(&pushed == &vector.back());
        std::string movable = "moved";
        std::string &movedIn = vector.push_back(std::move(movable));
        CHECK(movedIn == "moved");
        CHECK(vector.chunk_count() == 26);
        CHECK(&vector[0] == addresses[0]);
    }

    TEST_CASE("stable_vector - element access through operator[], at, front and back") {
        // Out-of-range at() is fatal (assert + terminate) and operator[]/front/back out of range
        // are undefined after the assert, so only in-range access can run inside the test process.
        stable_vector<int, 2> vector = {};
        vector.emplace_back(1);
        vector.emplace_back(2);
        vector.emplace_back(3);
        CHECK(vector[0] == 1);
        CHECK(vector.at(2) == 3);
        CHECK(vector.front() == 1);
        CHECK(vector.back() == 3);
        vector.front() = 10;
        vector.back() = 30;
        vector.at(1) = 20;
        vector[2] = 31;

        const stable_vector<int, 2> &constVector = vector;
        CHECK(constVector[0] == 10);
        CHECK(constVector.at(1) == 20);
        CHECK(constVector.front() == 10);
        CHECK(constVector.back() == 31);
        CHECK(&constVector.front() == &constVector[0]);
        CHECK(&constVector.back() == &constVector[2]);
    }

    TEST_CASE("stable_vector - pop_back destroys, clear keeps chunks, shrink_to_fit frees them") {
        testing::tracked_value::reset();
        stable_vector<testing::tracked_value, 4> vector = {};
        for (int value = 0; value < 10; ++value) {
            vector.emplace_back(value);
        }
        CHECK(vector.chunk_count() == 3);

        vector.pop_back();
        CHECK(vector.size() == 9);
        CHECK(testing::tracked_value::live_count() == 9);
        CHECK(vector.back().value() == 8);
        CHECK(vector.chunk_count() == 3);

        vector.clear();
        CHECK(vector.empty());
        CHECK(testing::tracked_value::live_count() == 0);
        CHECK(vector.chunk_count() == 3);
        CHECK(vector.capacity() == 12);

        vector.emplace_back(1);
        vector.shrink_to_fit();
        CHECK(vector.chunk_count() == 1);
        CHECK(vector.capacity() == 4);
        CHECK(vector[0].value() == 1);

        vector.clear();
        vector.shrink_to_fit();
        CHECK(vector.chunk_count() == 0);
        CHECK(vector.capacity() == 0);

        testing::expect_assert asserts = {};
        vector.pop_back();
        CHECK(asserts.count() == 1);
        CHECK(vector.empty());
    }

    TEST_CASE("stable_vector - reserve allocates whole chunks ahead of time") {
        stable_vector<int, 8> vector = {};
        vector.reserve(17);
        CHECK(vector.chunk_count() == 3);
        CHECK(vector.capacity() == 24);
        CHECK(vector.empty());
        vector.reserve(10);   // never shrinks
        CHECK(vector.capacity() == 24);

        for (int value = 0; value < 24; ++value) {
            vector.emplace_back(value);
        }
        CHECK(vector.chunk_count() == 3);
        vector.emplace_back(24);
        CHECK(vector.chunk_count() == 4);
    }

    TEST_CASE("stable_vector - iterators are random access and work with algorithms") {
        stable_vector<int, 4> vector = {};
        for (int value = 9; value >= 0; --value) {
            vector.emplace_back(value);
        }

        CHECK(std::distance(vector.begin(), vector.end()) == 10);
        CHECK(*(vector.begin() + 3) == 6);
        CHECK(vector.begin()[4] == 5);
        CHECK(*(vector.end() - 1) == 0);
        CHECK((vector.end() - vector.begin()) == 10);
        CHECK(vector.begin() < vector.end());
        CHECK(vector.begin() + 10 == vector.end());

        auto iterator = vector.begin();
        iterator += 2;
        CHECK(*iterator == 7);
        iterator -= 1;
        CHECK(*iterator == 8);
        CHECK(*iterator++ == 8);
        CHECK(*iterator == 7);
        CHECK(*--iterator == 8);
        CHECK(*(2 + vector.begin()) == 7);

        const auto found = std::find(vector.begin(), vector.end(), 4);
        REQUIRE(found != vector.end());
        CHECK(found - vector.begin() == 5);

        std::sort(vector.begin(), vector.end());
        CHECK(values_of(vector) == std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9});

        const stable_vector<int, 4> &constVector = vector;
        stable_vector<int, 4>::const_iterator converted = vector.begin();
        CHECK(converted == constVector.cbegin());
        CHECK(*converted == 0);
        CHECK(std::distance(constVector.begin(), constVector.end()) == 10);
        CHECK(constVector.cend() == constVector.end());

        for (int &value : vector) {
            value *= 2;
        }
        CHECK(vector[9] == 18);
        int sum = 0;
        for (const int value : constVector) {
            sum += value;
        }
        CHECK(sum == 90);
    }

    TEST_CASE("stable_vector - iterator ordering is only defined within one vector") {
        stable_vector<int, 4> a = {};
        stable_vector<int, 4> b = {};
        a.emplace_back(1);
        b.emplace_back(1);

        CHECK(a.begin() == a.begin());
        CHECK(a.begin() != a.end());
        CHECK((a.begin() <=> a.end()) == std::strong_ordering::less);
        CHECK((a.end() <=> a.begin()) == std::strong_ordering::greater);
        CHECK((a.begin() <=> a.begin()) == std::strong_ordering::equal);

        // Same index, different owner: never equal, and ordering is a precondition violation.
        CHECK_FALSE(a.begin() == b.begin());
        CHECK(a.begin() != b.begin());
        testing::expect_assert asserts = {};
        static_cast<void>(a.begin() <=> b.begin());
        CHECK(asserts.count() == 1);
        static_cast<void>(a.begin() < b.begin());
        CHECK(asserts.count() == 2);
        static_cast<void>(a.end() - b.begin());
        CHECK(asserts.count() == 3);
    }

    TEST_CASE("stable_vector - copy and move") {
        stable_vector<std::string, 2> source = {};
        source.emplace_back("a");
        source.emplace_back("b");
        source.emplace_back("c");
        const std::string *firstAddress = &source[0];

        stable_vector<std::string, 2> copy = source;
        CHECK(copy.size() == 3);
        CHECK(copy[2] == "c");
        CHECK(&copy[0] != firstAddress);
        copy[0] = "changed";
        CHECK(source[0] == "a");

        stable_vector<std::string, 2> moved = std::move(source);
        CHECK(moved.size() == 3);
        CHECK(&moved[0] == firstAddress);   // chunks are stolen, so even addresses survive
        CHECK(source.empty());
        CHECK(source.chunk_count() == 0);
        source.emplace_back("again");
        CHECK(source.size() == 1);

        stable_vector<std::string, 2> assigned = {};
        assigned.emplace_back("old");
        assigned = copy;
        CHECK(assigned.size() == 3);
        CHECK(assigned[0] == "changed");
        assigned = std::move(moved);
        CHECK(assigned.size() == 3);
        CHECK(assigned[0] == "a");
        CHECK(moved.empty());
        stable_vector<std::string, 2> &self = assigned;
        assigned = self;   // self assignment is harmless
        CHECK(assigned.size() == 3);
    }

    TEST_CASE("stable_vector - non-trivial elements are constructed and destroyed exactly once") {
        testing::tracked_value::reset();
        {
            testing::leak_check leaks = {};
            stable_vector<testing::tracked_value, 3> vector = {};
            for (int value = 0; value < 20; ++value) {
                vector.emplace_back(value);
            }
            stable_vector<testing::tracked_value, 3> copy = vector;
            stable_vector<testing::tracked_value, 3> moved = std::move(copy);
            moved.pop_back();
            vector.clear();
            CHECK(testing::tracked_value::live_count() == 19);
        }
        CHECK(testing::tracked_value::live_count() == 0);
        CHECK(testing::tracked_value::constructed_count() == testing::tracked_value::destroyed_count());
    }

    TEST_CASE("stable_vector - element types without a default constructor and move-only types") {
        stable_vector<no_default, 2> values = {};
        values.emplace_back(5);
        values.emplace_back(6);
        values.emplace_back(7);
        CHECK(values[2].value == 7);

        stable_vector<std::unique_ptr<int>, 2> pointers = {};
        pointers.emplace_back(std::make_unique<int>(1));
        pointers.push_back(std::make_unique<int>(2));
        CHECK(*pointers[1] == 2);
        stable_vector<std::unique_ptr<int>, 2> moved = std::move(pointers);
        CHECK(*moved[0] == 1);
    }

    TEST_CASE("stable_vector - chunk size of one and over-aligned elements") {
        stable_vector<int, 1> tiny = {};
        for (int value = 0; value < 5; ++value) {
            tiny.emplace_back(value);
        }
        CHECK(tiny.chunk_count() == 5);
        CHECK(values_of(tiny) == std::vector<int>{0, 1, 2, 3, 4});

        stable_vector<testing::wide_value, 3> wide = {};
        for (u64 value = 0; value < 7; ++value) {
            testing::wide_value &element = wide.emplace_back(testing::wide_value{value});
            CHECK(testing::is_aligned(&element, alignof(testing::wide_value)));
        }
        CHECK(wide[6].payload == 6);
    }

    TEST_CASE("stable_vector - property test against a std::vector oracle") {
        testing::tracked_value::reset();
        std::mt19937_64 random(0x5EED);
        {
            testing::leak_check leaks = {};
            stable_vector<testing::tracked_value, 5> vector = {};
            std::vector<int> oracle = {};
            std::vector<const testing::tracked_value *> addresses = {};

            for (int step = 0; step < 5000; ++step) {
                const u64 roll = random() % 100;
                const int value = static_cast<int>(random() % 1000);
                if (roll < 60) {
                    addresses.push_back(&vector.emplace_back(value));
                    oracle.push_back(value);
                } else if (roll < 90 && !oracle.empty()) {
                    vector.pop_back();
                    oracle.pop_back();
                    addresses.pop_back();
                } else if (roll < 95) {
                    vector.shrink_to_fit();
                } else if (roll < 98) {
                    vector.reserve(static_cast<usize>(random() % 50));
                } else {
                    vector.clear();
                    oracle.clear();
                    addresses.clear();
                }

                REQUIRE(vector.size() == oracle.size());
                REQUIRE(values_of(vector) == oracle);
                REQUIRE(testing::tracked_value::live_count() == static_cast<i64>(oracle.size()));
                REQUIRE(vector.capacity() >= vector.size());
                REQUIRE(vector.capacity() % 5 == 0);
                for (usize index = 0; index < addresses.size(); ++index) {
                    REQUIRE(&vector[index] == addresses[index]);
                }
            }
        }
        CHECK(testing::tracked_value::live_count() == 0);
    }
}
