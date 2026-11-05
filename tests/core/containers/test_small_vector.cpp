#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"

#include "tracked_value.h"

#include "conjure/core/containers/small_vector.h"

#include <algorithm>
#include <compare>
#include <iterator>
#include <list>
#include <memory>
#include <numeric>
#include <random>
#include <span>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;

namespace {
    /// <summary>
    /// Orderable through operator< only, like many pre-C++20 types.
    /// </summary>
    struct only_less {
        int value = 0;

        [[nodiscard]] bool operator<(const only_less &other) const noexcept {
            return value < other.value;
        }

        [[nodiscard]] bool operator==(const only_less &other) const noexcept {
            return value == other.value;
        }
    };

    static_assert(!std::three_way_comparable<only_less>);

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

TEST_SUITE("core/small_vector") {

    TEST_CASE("small_vector - default constructed is empty and inline") {
        small_vector<int, 4> vector = {};
        CHECK(vector.empty());
        CHECK(vector.size() == 0);
        CHECK(vector.capacity() == 4);
        CHECK(vector.is_inline());
        CHECK(vector.begin() == vector.end());
        CHECK(vector.data() != nullptr);
        CHECK(small_vector<int, 4>::INLINE_CAPACITY == 4);
        CHECK(small_vector<int, 4>::IS_TRIVIALLY_RELOCATABLE);
        CHECK_FALSE(small_vector<std::string, 4>::IS_TRIVIALLY_RELOCATABLE);
        CHECK(small_vector<int, 4>::max_size() > 1000000);
    }

    TEST_CASE("small_vector - push_back stays inline up to N and moves to the heap beyond") {
        small_vector<int, 3> vector = {};
        vector.push_back(1);
        vector.push_back(2);
        int three = 3;
        vector.push_back(three);
        CHECK(vector.is_inline());
        CHECK(vector.size() == 3);
        CHECK(vector.capacity() == 3);

        const int *inlineData = vector.data();
        vector.push_back(4);
        CHECK_FALSE(vector.is_inline());
        CHECK(vector.size() == 4);
        CHECK(vector.capacity() >= 4);
        CHECK(vector.data() != inlineData);
        CHECK(values_of(vector) == std::vector<int>{1, 2, 3, 4});

        for (int value = 5; value <= 100; ++value) {
            vector.push_back(value);
        }
        CHECK(vector.size() == 100);
        for (int value = 1; value <= 100; ++value) {
            CAPTURE(value);
            CHECK(vector[static_cast<usize>(value - 1)] == value);
        }
    }

    TEST_CASE("small_vector - emplace_back returns the new element and tolerates aliasing arguments") {
        small_vector<std::string, 2> vector = {};
        std::string &first = vector.emplace_back(3, 'a');
        CHECK(first == "aaa");
        CHECK(&first == &vector.back());

        vector.emplace_back("bb");
        // Growth happens here; the argument refers to an element of the old buffer.
        vector.push_back(vector[0]);
        CHECK(vector.size() == 3);
        CHECK(vector[2] == "aaa");
        vector.emplace_back(vector[1]);
        CHECK(vector[3] == "bb");

        small_vector<int, 1> ints = {5};
        ints.push_back(ints[0]);
        ints.push_back(ints[1]);
        CHECK(values_of(ints) == std::vector<int>{5, 5, 5});
    }

    TEST_CASE("small_vector - pop_back destroys the last element and asserts when empty") {
        testing::tracked_value::reset();
        small_vector<testing::tracked_value, 4> vector = {};
        vector.emplace_back(1);
        vector.emplace_back(2);
        vector.pop_back();
        CHECK(vector.size() == 1);
        CHECK(testing::tracked_value::live_count() == 1);
        CHECK(vector.back().value() == 1);
        vector.pop_back();
        CHECK(vector.empty());
        CHECK(testing::tracked_value::live_count() == 0);

        testing::expect_assert asserts = {};
        vector.pop_back();
        CHECK(asserts.count() == 1);
        CHECK(vector.empty());
    }

    TEST_CASE("small_vector - insert at begin, middle and end for trivial elements") {
        small_vector<int, 4> vector = {1, 2, 3};
        auto position = vector.insert(vector.begin(), 0);
        CHECK(*position == 0);
        CHECK(values_of(vector) == std::vector<int>{0, 1, 2, 3});
        CHECK(vector.is_inline());

        position = vector.insert(vector.begin() + 2, 9);   // grows
        CHECK(*position == 9);
        CHECK(values_of(vector) == std::vector<int>{0, 1, 9, 2, 3});
        CHECK_FALSE(vector.is_inline());

        position = vector.insert(vector.end(), 4);
        CHECK(*position == 4);
        CHECK(values_of(vector) == std::vector<int>{0, 1, 9, 2, 3, 4});

        int moved = 7;
        position = vector.insert(vector.begin() + 1, std::move(moved));
        CHECK(*position == 7);
        CHECK(values_of(vector) == std::vector<int>{0, 7, 1, 9, 2, 3, 4});
    }

    TEST_CASE("small_vector - insert in the middle for non-trivial elements keeps every element intact") {
        testing::tracked_value::reset();
        {
            testing::leak_check leaks = {};
            small_vector<testing::tracked_value, 4> vector = {};
            vector.emplace_back(1);
            vector.emplace_back(3);
            vector.emplace_back(4);

            vector.insert(vector.begin() + 1, testing::tracked_value(2));   // inline shift
            CHECK(values_of(vector) == std::vector<int>{1, 2, 3, 4});
            CHECK(vector.is_inline());

            vector.emplace(vector.begin(), 0);   // grow with hole
            CHECK(values_of(vector) == std::vector<int>{0, 1, 2, 3, 4});
            CHECK_FALSE(vector.is_inline());

            vector.emplace(vector.begin() + 2, vector[4]);   // aliasing argument, heap shift
            CHECK(values_of(vector) == std::vector<int>{0, 1, 4, 2, 3, 4});
            CHECK(testing::tracked_value::live_count() == 6);
        }
        CHECK(testing::tracked_value::live_count() == 0);
    }

    TEST_CASE("small_vector - insert of a range and an initializer list") {
        small_vector<int, 4> vector = {1, 5};
        const std::vector<int> middle = {2, 3, 4};
        auto position = vector.insert(vector.begin() + 1, middle.begin(), middle.end());
        CHECK(*position == 2);
        CHECK(values_of(vector) == std::vector<int>{1, 2, 3, 4, 5});

        position = vector.insert(vector.end(), {6, 7});
        CHECK(*position == 6);
        CHECK(values_of(vector) == std::vector<int>{1, 2, 3, 4, 5, 6, 7});

        position = vector.insert(vector.begin(), middle.begin(), middle.begin());   // empty range
        CHECK(position == vector.begin());
        CHECK(vector.size() == 7);

        small_vector<std::string, 2> strings = {"a", "d"};
        const std::list<std::string> inserted = {"b", "c"};
        strings.insert(strings.begin() + 1, inserted.begin(), inserted.end());
        CHECK(strings.size() == 4);
        CHECK(strings[0] == "a");
        CHECK(strings[1] == "b");
        CHECK(strings[2] == "c");
        CHECK(strings[3] == "d");
    }

    TEST_CASE("small_vector - insert accepts single-pass input iterators") {
        static_assert(std::input_iterator<std::istream_iterator<int>>);
        static_assert(!std::forward_iterator<std::istream_iterator<int>>);

        small_vector<int, 4> vector = {1, 5};
        std::istringstream numbers("2 3 4");
        auto position = vector.insert(vector.begin() + 1, std::istream_iterator<int>(numbers), {});
        CHECK(*position == 2);
        CHECK(values_of(vector) == std::vector<int>{1, 2, 3, 4, 5});
        CHECK_FALSE(vector.is_inline());

        small_vector<std::string, 2> strings = {"a", "d"};
        std::istringstream words("b c");
        strings.insert(strings.end() - 1, std::istream_iterator<std::string>(words), {});
        REQUIRE(strings.size() == 4);
        CHECK(strings[0] == "a");
        CHECK(strings[1] == "b");
        CHECK(strings[2] == "c");
        CHECK(strings[3] == "d");

        std::istringstream empty("");
        position = vector.insert(vector.begin(), std::istream_iterator<int>(empty), {});
        CHECK(position == vector.begin());
        CHECK(vector.size() == 5);
    }

    TEST_CASE("small_vector - emplace and insert past end() assert and leave the vector unchanged") {
        // Capacity 8 keeps the bad positions inside the inline array, so forming them is legal.
        small_vector<int, 8> vector = {1, 2, 3};
        const std::vector<int> more = {7, 8};
        testing::expect_assert asserts = {};

        CHECK(vector.emplace(vector.end() + 1, 9) == vector.end());
        CHECK(asserts.count() == 1);
        CHECK(vector.insert(vector.begin() + 5, 4) == vector.end());
        CHECK(asserts.count() == 2);
        CHECK(vector.insert(vector.end() + 2, more.begin(), more.end()) == vector.end());
        CHECK(asserts.count() == 3);
        CHECK(vector.insert(vector.end() + 1, {5, 6}) == vector.end());
        CHECK(asserts.count() == 4);

        CHECK(values_of(vector) == std::vector<int>{1, 2, 3});
        CHECK(vector.is_inline());

        // end() itself is a valid position for both.
        CHECK(*vector.emplace(vector.end(), 4) == 4);
        CHECK(*vector.insert(vector.end(), more.begin(), more.end()) == 7);
        CHECK(values_of(vector) == std::vector<int>{1, 2, 3, 4, 7, 8});
        CHECK(asserts.count() == 4);
    }

    TEST_CASE("small_vector - erase single elements and ranges, returning the following iterator") {
        small_vector<int, 8> vector = {0, 1, 2, 3, 4, 5, 6, 7};
        auto next = vector.erase(vector.begin() + 2);
        CHECK(*next == 3);
        CHECK(values_of(vector) == std::vector<int>{0, 1, 3, 4, 5, 6, 7});

        next = vector.erase(vector.begin() + 1, vector.begin() + 4);
        CHECK(*next == 5);
        CHECK(values_of(vector) == std::vector<int>{0, 5, 6, 7});

        next = vector.erase(vector.end() - 1);
        CHECK(next == vector.end());
        CHECK(values_of(vector) == std::vector<int>{0, 5, 6});

        next = vector.erase(vector.begin(), vector.begin());   // empty range
        CHECK(next == vector.begin());
        CHECK(vector.size() == 3);

        next = vector.erase(vector.begin(), vector.end());
        CHECK(next == vector.end());
        CHECK(vector.empty());

        testing::tracked_value::reset();
        {
            testing::leak_check leaks = {};
            small_vector<testing::tracked_value, 2> tracked = {};
            for (int value = 0; value < 6; ++value) {
                tracked.emplace_back(value);
            }
            tracked.erase(tracked.begin() + 1, tracked.begin() + 3);
            CHECK(values_of(tracked) == std::vector<int>{0, 3, 4, 5});
            CHECK(testing::tracked_value::live_count() == 4);
            tracked.erase(tracked.begin());
            CHECK(values_of(tracked) == std::vector<int>{3, 4, 5});
            CHECK(testing::tracked_value::live_count() == 3);
        }
        CHECK(testing::tracked_value::live_count() == 0);
    }

    TEST_CASE("small_vector - resize grows with value-initialised or given elements and shrinks") {
        small_vector<int, 2> vector = {};
        vector.resize(3);
        CHECK(values_of(vector) == std::vector<int>{0, 0, 0});
        vector.resize(5, 7);
        CHECK(values_of(vector) == std::vector<int>{0, 0, 0, 7, 7});
        vector.resize(1);
        CHECK(values_of(vector) == std::vector<int>{0});
        vector.resize(0);
        CHECK(vector.empty());

        testing::tracked_value::reset();
        small_vector<testing::tracked_value, 2> tracked = {};
        tracked.resize(10);
        CHECK(testing::tracked_value::live_count() == 10);
        tracked.resize(3, testing::tracked_value(1));
        CHECK(testing::tracked_value::live_count() == 3);
        tracked.resize(4, testing::tracked_value(1));
        CHECK(tracked[3].value() == 1);
        CHECK(testing::tracked_value::live_count() == 4);
    }

    TEST_CASE("small_vector - reserve and shrink_to_fit move between inline and heap storage") {
        small_vector<int, 4> vector = {1, 2};
        vector.reserve(2);
        CHECK(vector.is_inline());
        vector.reserve(16);
        CHECK_FALSE(vector.is_inline());
        CHECK(vector.capacity() >= 16);
        CHECK(values_of(vector) == std::vector<int>{1, 2});

        vector.shrink_to_fit();
        CHECK(vector.is_inline());
        CHECK(vector.capacity() == 4);
        CHECK(values_of(vector) == std::vector<int>{1, 2});

        for (int value = 3; value <= 10; ++value) {
            vector.push_back(value);
        }
        vector.reserve(100);
        vector.shrink_to_fit();
        CHECK_FALSE(vector.is_inline());
        CHECK(vector.capacity() == 10);
        CHECK(vector.size() == 10);
        vector.shrink_to_fit();   // already tight
        CHECK(vector.capacity() == 10);
    }

    TEST_CASE("small_vector - clear destroys the elements and keeps the storage") {
        testing::tracked_value::reset();
        small_vector<testing::tracked_value, 2> vector = {};
        for (int value = 0; value < 5; ++value) {
            vector.emplace_back(value);
        }
        const usize capacity = vector.capacity();
        vector.clear();
        CHECK(vector.empty());
        CHECK(vector.capacity() == capacity);
        CHECK(testing::tracked_value::live_count() == 0);
        vector.emplace_back(1);
        CHECK(vector.size() == 1);
    }

    TEST_CASE("small_vector - element access through operator[], at, front and back") {
        // Out-of-range at() is fatal (assert + terminate) and operator[]/front/back out of range
        // are undefined after the assert, so only in-range access can run inside the test process.
        small_vector<int, 4> vector = {1, 2, 3};
        CHECK(vector[0] == 1);
        CHECK(vector.at(2) == 3);
        CHECK(vector.front() == 1);
        CHECK(vector.back() == 3);
        vector.front() = 10;
        vector.back() = 30;
        vector.at(1) = 20;
        vector[2] = 31;
        const small_vector<int, 4> &constVector = vector;
        CHECK(constVector[0] == 10);
        CHECK(constVector.at(1) == 20);
        CHECK(constVector.front() == 10);
        CHECK(constVector.back() == 31);
        CHECK(&constVector.front() == constVector.data());
        CHECK(&constVector.back() == constVector.data() + 2);
    }

    TEST_CASE("small_vector - iterators, reverse iterators and span conversion") {
        small_vector<int, 4> vector = {1, 2, 3, 4, 5};
        CHECK(std::accumulate(vector.begin(), vector.end(), 0) == 15);
        CHECK(std::accumulate(vector.cbegin(), vector.cend(), 0) == 15);
        std::vector<int> reversed(vector.rbegin(), vector.rend());
        CHECK(reversed == std::vector<int>{5, 4, 3, 2, 1});

        std::span<int> mutableView = vector;
        mutableView[0] = 100;
        CHECK(vector[0] == 100);
        std::span<const int> constView = vector;
        CHECK(constView.size() == 5);
        CHECK(constView[0] == 100);

        const small_vector<int, 4> &constVector = vector;
        std::span<const int> fromConst = constVector;
        CHECK(fromConst.data() == constVector.data());
        std::vector<int> fromConstReverse(constVector.rbegin(), constVector.rend());
        CHECK(fromConstReverse.front() == 5);

        std::sort(vector.begin(), vector.end());
        CHECK(values_of(vector) == std::vector<int>{2, 3, 4, 5, 100});
    }

    TEST_CASE("small_vector - copy construction and assignment deep-copy inline and heap contents") {
        small_vector<std::string, 2> inlineSource = {"a", "b"};
        small_vector<std::string, 2> inlineCopy = inlineSource;
        CHECK(inlineCopy.is_inline());
        CHECK(inlineCopy == inlineSource);
        inlineCopy[0] = "changed";
        CHECK(inlineSource[0] == "a");

        small_vector<std::string, 2> heapSource = {"a", "b", "c", "d"};
        small_vector<std::string, 2> heapCopy = heapSource;
        CHECK_FALSE(heapCopy.is_inline());
        CHECK(heapCopy == heapSource);
        CHECK(heapCopy.data() != heapSource.data());

        heapCopy = inlineSource;
        CHECK(heapCopy == inlineSource);
        inlineCopy = heapSource;
        CHECK(inlineCopy == heapSource);
        small_vector<std::string, 2> &self = heapCopy;
        heapCopy = self;   // self assignment is harmless
        CHECK(heapCopy == inlineSource);
    }

    TEST_CASE("small_vector - move steals heap storage and relocates inline elements") {
        small_vector<std::string, 2> heapSource = {"a", "b", "c"};
        const std::string *heapData = heapSource.data();
        small_vector<std::string, 2> heapMoved = std::move(heapSource);
        CHECK(heapMoved.data() == heapData);
        CHECK(heapMoved.size() == 3);
        CHECK(heapSource.empty());
        CHECK(heapSource.is_inline());
        CHECK(heapSource.capacity() == 2);

        small_vector<std::string, 2> inlineSource = {"x", "y"};
        small_vector<std::string, 2> inlineMoved = std::move(inlineSource);
        CHECK(inlineMoved.is_inline());
        CHECK(inlineMoved.size() == 2);
        CHECK(inlineMoved[0] == "x");
        CHECK(inlineSource.empty());

        heapMoved = std::move(inlineMoved);
        CHECK(heapMoved.is_inline());
        CHECK(heapMoved.size() == 2);
        CHECK(heapMoved[1] == "y");
        CHECK(inlineMoved.empty());

        small_vector<std::string, 2> another = {"1", "2", "3", "4"};
        inlineMoved = std::move(another);
        CHECK_FALSE(inlineMoved.is_inline());
        CHECK(inlineMoved.size() == 4);
        CHECK(another.empty());
        CHECK(another.is_inline());

        // The moved-from vector is fully usable.
        another.push_back("again");
        CHECK(another.size() == 1);
    }

    TEST_CASE("small_vector - move-only element types") {
        small_vector<std::unique_ptr<int>, 2> vector = {};
        vector.push_back(std::make_unique<int>(1));
        vector.emplace_back(std::make_unique<int>(2));
        vector.push_back(std::make_unique<int>(3));   // grows: relocation moves the pointers
        vector.insert(vector.begin(), std::make_unique<int>(0));
        REQUIRE(vector.size() == 4);
        for (int value = 0; value < 4; ++value) {
            CAPTURE(value);
            REQUIRE(vector[static_cast<usize>(value)] != nullptr);
            CHECK(*vector[static_cast<usize>(value)] == value);
        }
        vector.erase(vector.begin() + 1);
        CHECK(*vector[1] == 2);

        small_vector<std::unique_ptr<int>, 2> moved = std::move(vector);
        CHECK(moved.size() == 3);
        CHECK(vector.empty());
    }

    TEST_CASE("small_vector - non-trivial elements are never leaked or double destroyed") {
        testing::tracked_value::reset();
        {
            testing::leak_check leaks = {};
            small_vector<testing::tracked_value, 4> vector = {};
            for (int value = 0; value < 50; ++value) {
                vector.emplace_back(value);
            }
            small_vector<testing::tracked_value, 4> copy = vector;
            small_vector<testing::tracked_value, 4> moved = std::move(copy);
            moved.erase(moved.begin() + 10, moved.begin() + 20);
            moved.insert(moved.begin() + 5, vector.begin(), vector.begin() + 7);
            moved.resize(3);
            moved.shrink_to_fit();
            CHECK(moved.is_inline());
            vector = moved;
            CHECK(testing::tracked_value::live_count() == 6);
        }
        CHECK(testing::tracked_value::live_count() == 0);
        CHECK(testing::tracked_value::constructed_count() == testing::tracked_value::destroyed_count());
    }

    TEST_CASE("small_vector - equality and lexicographical ordering") {
        const small_vector<int, 2> a = {1, 2, 3};
        const small_vector<int, 2> b = {1, 2, 3};
        const small_vector<int, 2> c = {1, 2, 4};
        const small_vector<int, 2> shorter = {1, 2};
        CHECK(a == b);
        CHECK(a != c);
        CHECK(a < c);
        CHECK(shorter < a);
        CHECK(c > a);
        CHECK((a <=> b) == std::strong_ordering::equal);
        static_assert(std::is_same_v<decltype(a <=> b), std::strong_ordering>);
    }

    TEST_CASE("small_vector - ordering falls back to operator< for types without operator<=>") {
        const small_vector<only_less, 2> a = {only_less{1}, only_less{2}};
        const small_vector<only_less, 2> b = {only_less{1}, only_less{3}};
        const small_vector<only_less, 2> shorter = {only_less{1}};
        const small_vector<only_less, 2> same = {only_less{1}, only_less{2}};
        static_assert(std::is_same_v<decltype(a <=> b), std::weak_ordering>);

        CHECK(a < b);
        CHECK(b > a);
        CHECK(a <= same);
        CHECK(a >= same);
        CHECK(shorter < a);
        CHECK(a == same);
        CHECK(a != b);
        CHECK((a <=> b) == std::weak_ordering::less);
        CHECK((a <=> same) == std::weak_ordering::equivalent);
        CHECK((b <=> shorter) == std::weak_ordering::greater);

        // Types that are not orderable at all still make a usable (just unordered) small_vector.
        static_assert(!std::three_way_comparable<small_vector<testing::wide_value, 2>>);
        static_assert(std::three_way_comparable<small_vector<only_less, 2>>);
    }

    TEST_CASE("small_vector - constructors from count, count and value, iterator range, span and list") {
        const small_vector<int, 4> counted(3);
        CHECK(values_of(counted) == std::vector<int>{0, 0, 0});

        const small_vector<int, 2> filled(5, 9);
        CHECK(values_of(filled) == std::vector<int>{9, 9, 9, 9, 9});
        CHECK_FALSE(filled.is_inline());

        const std::vector<int> source = {4, 5, 6};
        const small_vector<int, 4> fromRange(source.begin(), source.end());
        CHECK(values_of(fromRange) == source);

        const std::list<int> linked = {7, 8};
        const small_vector<int, 4> fromList(linked.begin(), linked.end());
        CHECK(values_of(fromList) == std::vector<int>{7, 8});

        const std::span<const int> sourceView(source);
        const small_vector<int, 4> fromSpan(sourceView);
        CHECK(values_of(fromSpan) == source);

        small_vector<int, 4> assigned = {};
        assigned = {1, 2};
        CHECK(values_of(assigned) == std::vector<int>{1, 2});
        assigned.assign(3, 4);
        CHECK(values_of(assigned) == std::vector<int>{4, 4, 4});
        assigned.assign(source.begin(), source.end());
        CHECK(values_of(assigned) == source);

        const small_vector<int, 4> braceOne{3};   // initializer list wins over the count constructor
        CHECK(values_of(braceOne) == std::vector<int>{3});
    }

    TEST_CASE("small_vector - swap exchanges inline and heap contents") {
        small_vector<std::string, 2> inlineVector = {"a"};
        small_vector<std::string, 2> heapVector = {"1", "2", "3"};
        inlineVector.swap(heapVector);
        CHECK(inlineVector.size() == 3);
        CHECK(inlineVector[2] == "3");
        CHECK(heapVector.size() == 1);
        CHECK(heapVector[0] == "a");
        CHECK(heapVector.is_inline());

        swap(inlineVector, heapVector);
        CHECK(inlineVector.size() == 1);
        CHECK(heapVector.size() == 3);
        inlineVector.swap(inlineVector);
        CHECK(inlineVector.size() == 1);
    }

    TEST_CASE("small_vector - over-aligned elements are stored aligned inline and on the heap") {
        small_vector<testing::wide_value, 2> vector = {};
        vector.push_back(testing::wide_value{1});
        CHECK(testing::is_aligned(vector.data(), alignof(testing::wide_value)));
        CHECK(vector.is_inline());
        vector.push_back(testing::wide_value{2});
        vector.push_back(testing::wide_value{3});
        CHECK_FALSE(vector.is_inline());
        CHECK(testing::is_aligned(vector.data(), alignof(testing::wide_value)));
        CHECK(vector[2].payload == 3);
    }

    TEST_CASE("small_vector - property test against a std::vector oracle") {
        testing::tracked_value::reset();
        std::mt19937_64 random(0x5EED);
        {
            testing::leak_check leaks = {};
            small_vector<testing::tracked_value, 8> vector = {};
            std::vector<int> oracle = {};

            for (int step = 0; step < 5000; ++step) {
                const u64 roll = random() % 100;
                const int value = static_cast<int>(random() % 1000);
                if (roll < 35) {
                    vector.emplace_back(value);
                    oracle.push_back(value);
                } else if (roll < 50 && !oracle.empty()) {
                    vector.pop_back();
                    oracle.pop_back();
                } else if (roll < 65) {
                    const usize index = static_cast<usize>(random() % (oracle.size() + 1));
                    vector.insert(vector.begin() + static_cast<isize>(index), testing::tracked_value(value));
                    oracle.insert(oracle.begin() + static_cast<isize>(index), value);
                } else if (roll < 80 && !oracle.empty()) {
                    const usize first = static_cast<usize>(random() % oracle.size());
                    const usize last = first + static_cast<usize>(random() % (oracle.size() - first + 1));
                    vector.erase(vector.begin() + static_cast<isize>(first), vector.begin() + static_cast<isize>(last));
                    oracle.erase(oracle.begin() + static_cast<isize>(first), oracle.begin() + static_cast<isize>(last));
                } else if (roll < 90) {
                    const usize newSize = static_cast<usize>(random() % 40);
                    vector.resize(newSize, testing::tracked_value(value));
                    oracle.resize(newSize, value);
                } else if (roll < 95) {
                    vector.shrink_to_fit();
                } else if (roll < 98) {
                    small_vector<testing::tracked_value, 8> copy = vector;
                    vector = std::move(copy);
                } else {
                    vector.reserve(static_cast<usize>(random() % 64));
                }

                REQUIRE(vector.size() == oracle.size());
                REQUIRE(values_of(vector) == oracle);
                REQUIRE(testing::tracked_value::live_count() == static_cast<i64>(oracle.size()));
                REQUIRE(vector.is_inline() == (vector.capacity() == 8));
            }
        }
        CHECK(testing::tracked_value::live_count() == 0);
    }
}
