#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"

#include "conjure/core/delegate.h"

#include <array>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

using namespace conjure;
using namespace conjure::core;

namespace {
    int sFreeCalls = 0;

    int add_one(int value) {
        ++sFreeCalls;
        return value + 1;
    }

    int add_two(int value) {
        return value + 2;
    }

    void increment(int &value) {
        ++value;
    }

    long widen(int value) {
        return static_cast<long>(value) * 2;
    }

    class counter {
        private:
            int mTotal = 0;

        public:
            counter() = default;
            explicit counter(int start) :
                mTotal(start) {}

        public:
            void add(int amount) {
                mTotal += amount;
            }

            [[nodiscard]] int total() const {
                return mTotal;
            }

            [[nodiscard]] int scaled(int factor) const {
                return mTotal * factor;
            }
    };

    struct not_default_constructible {
        int value = 0;

        explicit not_default_constructible(int startValue) :
            value(startValue) {}
    };

    not_default_constructible make_value(int startValue) {
        return not_default_constructible(startValue);
    }
}

TEST_SUITE("core/delegate") {

    TEST_CASE("delegate - a default constructed delegate is unbound") {
        const delegate<void(int)> unbound = {};

        CHECK_FALSE(unbound.is_bound());
        CHECK_FALSE(static_cast<bool>(unbound));
        CHECK(unbound == delegate<void(int)>());
    }

    TEST_CASE("delegate - a delegate is two pointers of storage plus the invoker and is trivially copyable") {
        CHECK(sizeof(delegate<void()>) == 3 * sizeof(void *));
        CHECK(delegate<void()>::STORAGE_BYTES == 2 * sizeof(void *));
        static_assert(std::is_trivially_copyable_v<delegate<int(int)>>);
        static_assert(std::is_trivially_destructible_v<delegate<int(int)>>);
    }

    TEST_CASE("delegate - binds a free function at compile time") {
        const delegate<int(int)> bound = delegate<int(int)>::bind<&add_one>();
        sFreeCalls = 0;

        CHECK(bound.is_bound());
        CHECK(bound(1) == 2);
        CHECK(bound(41) == 42);
        CHECK(sFreeCalls == 2);
    }

    TEST_CASE("delegate - binds a function pointer at runtime") {
        int (*pointer)(int) = &add_two;
        const delegate<int(int)> bound = delegate<int(int)>::bind(pointer);

        CHECK(bound(1) == 3);

        SUBCASE("a null function pointer yields an unbound delegate") {
            pointer = nullptr;
            const delegate<int(int)> empty = delegate<int(int)>::bind(pointer);
            CHECK_FALSE(empty.is_bound());
        }
    }

    TEST_CASE("delegate - binds member functions to an instance") {
        counter instance(10);
        const delegate<void(int)> add = delegate<void(int)>::bind<&counter::add>(&instance);

        add(5);
        add(6);
        CHECK(instance.total() == 21);

        SUBCASE("const member functions accept a pointer to const") {
            const counter *constInstance = &instance;
            const delegate<int(int)> scaled = delegate<int(int)>::bind<&counter::scaled>(constInstance);
            CHECK(scaled(2) == 42);
        }

        SUBCASE("const member functions also accept a mutable pointer") {
            const delegate<int()> total = delegate<int()>::bind<&counter::total>(&instance);
            CHECK(total() == 21);
        }

        SUBCASE("binding a null instance asserts and yields an unbound delegate") {
            testing::expect_assert asserts = {};
            counter *missing = nullptr;
            const delegate<void(int)> empty = delegate<void(int)>::bind<&counter::add>(missing);
            CHECK(asserts.count() == 1);
            CHECK_FALSE(empty.is_bound());
        }
    }

    TEST_CASE("delegate - binds stateless lambdas") {
        const delegate<int(int, int)> multiply = delegate<int(int, int)>::bind([](int left, int right) {
            return left * right;
        });

        CHECK(multiply(6, 7) == 42);
    }

    TEST_CASE("delegate - binds lambdas capturing one or two pointers") {
        int total = 0;
        int calls = 0;
        const delegate<void(int)> accumulate = delegate<void(int)>::bind([&total, &calls](int amount) {
            total += amount;
            ++calls;
        });

        accumulate(3);
        accumulate(4);
        CHECK(total == 7);
        CHECK(calls == 2);
    }

    TEST_CASE("delegate - CAN_BIND reports which callables fit") {
        const auto stateless = [](int) {};
        int a = 0;
        int b = 0;
        const auto twoPointers = [&a, &b](int) {
            return a + b;
        };
        const auto threePointers = [&a, &b, pointer = &a](int) {
            return a + b + *pointer;
        };
        const std::string text = "owned";
        const auto ownsString = [text](int) {
            return static_cast<int>(text.size());
        };
        const auto wrongSignature = [](const std::string &) {};
        const auto mutableLambda = [count = 0](int) mutable {
            ++count;
            return count;
        };

        // Fits the storage by size but demands more alignment than the storage guarantees.
        struct alignas(2 * alignof(void *)) over_aligned {
            std::array<std::byte, 2 * alignof(void *)> payload = {};

            void operator()(int) const {}
        };
        static_assert(sizeof(over_aligned) <= delegate<void(int)>::STORAGE_BYTES);

        CHECK(delegate<void(int)>::CAN_BIND<decltype(stateless)>);
        CHECK(delegate<int(int)>::CAN_BIND<decltype(twoPointers)>);
        CHECK_FALSE(delegate<int(int)>::CAN_BIND<decltype(threePointers)>);
        CHECK_FALSE(delegate<int(int)>::CAN_BIND<decltype(ownsString)>);
        CHECK_FALSE(delegate<void(int)>::CAN_BIND<decltype(wrongSignature)>);
        CHECK_FALSE(delegate<int(int)>::CAN_BIND<decltype(mutableLambda)>);
        CHECK_FALSE(delegate<void(int)>::CAN_BIND<over_aligned>);
        CHECK(delegate<void(int)>::CAN_BIND<int (*)(int)>);
    }

    TEST_CASE("delegate - a bound delegate converts to true and differs from an unbound one") {
        const delegate<int(int)> bound = delegate<int(int)>::bind<&add_one>();
        const delegate<int(int)> unbound = {};

        CHECK(static_cast<bool>(bound));
        CHECK(bound != unbound);
        CHECK_FALSE(bound != delegate<int(int)>::bind<&add_one>());
    }

    TEST_CASE("delegate - two bindings of the same stateless lambda compare equal") {
        // Regression: the indeterminate byte of an empty callable used to be copied into the
        // storage, so equality depended on stack garbage.
        const auto negate = [](int value) {
            return -value;
        };
        const delegate<int(int)> first = delegate<int(int)>::bind(negate);
        const delegate<int(int)> second = delegate<int(int)>::bind(negate);

        CHECK(first == second);
        CHECK_FALSE(first != second);
        CHECK(first(2) == -2);
        CHECK(second(3) == -3);
    }

    TEST_CASE("delegate - arguments are forwarded by reference when the signature says so") {
        const delegate<void(int &)> bump = delegate<void(int &)>::bind<&increment>();
        int value = 1;

        bump(value);
        bump(value);
        CHECK(value == 3);

        const delegate<void(int &)> twice = delegate<void(int &)>::bind([](int &target) {
            target *= 2;
        });
        twice(value);
        CHECK(value == 6);
    }

    TEST_CASE("delegate - return values convert to the delegate's return type") {
        const delegate<long(int)> wide = delegate<long(int)>::bind<&widen>();
        CHECK(wide(21) == 42L);

        // A callable returning int bound to a delegate returning long.
        const delegate<long(int)> fromInt = delegate<long(int)>::bind([](int value) {
            return value + 1;
        });
        CHECK(fromInt(1) == 2L);

        // A callable with a return value bound to a void delegate discards it.
        const delegate<void(int)> discarding = delegate<void(int)>::bind<&add_two>();
        discarding(1);
        CHECK(discarding.is_bound());
    }

    TEST_CASE("delegate - reset unbinds") {
        delegate<int(int)> bound = delegate<int(int)>::bind<&add_one>();
        REQUIRE(bound.is_bound());

        bound.reset();
        CHECK_FALSE(bound.is_bound());
        CHECK(bound == delegate<int(int)>());
    }

    TEST_CASE("delegate - invoking an unbound delegate asserts and returns a default value") {
        testing::expect_assert asserts = {};
        const delegate<int(int)> unboundInt = {};
        const delegate<void(int)> unboundVoid = {};

        CHECK(unboundInt(5) == 0);
        CHECK(asserts.count() == 1);

        unboundVoid(5);
        CHECK(asserts.count() == 2);
    }

    TEST_CASE("delegate - delegates returning non default constructible types still work when bound") {
        using factory_delegate = delegate<not_default_constructible(int)>;
        const factory_delegate bound = factory_delegate::bind<&make_value>();
        CHECK(bound(9).value == 9);
    }

    TEST_CASE("delegate - equality compares target and instance") {
        counter first = {};
        counter second = {};

        const delegate<void(int)> firstAdd = delegate<void(int)>::bind<&counter::add>(&first);
        const delegate<void(int)> firstAddAgain = delegate<void(int)>::bind<&counter::add>(&first);
        const delegate<void(int)> secondAdd = delegate<void(int)>::bind<&counter::add>(&second);

        CHECK(firstAdd == firstAddAgain);
        CHECK(firstAdd != secondAdd);

        const delegate<int(int)> freeOne = delegate<int(int)>::bind<&add_one>();
        const delegate<int(int)> freeOneAgain = delegate<int(int)>::bind<&add_one>();
        const delegate<int(int)> freeTwo = delegate<int(int)>::bind<&add_two>();
        CHECK(freeOne == freeOneAgain);
        CHECK(freeOne != freeTwo);

        const delegate<int(int)> runtimeOne = delegate<int(int)>::bind(&add_one);
        const delegate<int(int)> runtimeOneAgain = delegate<int(int)>::bind(&add_one);
        const delegate<int(int)> runtimeTwo = delegate<int(int)>::bind(&add_two);
        CHECK(runtimeOne == runtimeOneAgain);
        CHECK(runtimeOne != runtimeTwo);

        int captured = 0;
        const auto lambda = [&captured](int) {
            ++captured;
        };
        const delegate<void(int)> lambdaOne = delegate<void(int)>::bind(lambda);
        const delegate<void(int)> lambdaTwo = delegate<void(int)>::bind(lambda);
        CHECK(lambdaOne == lambdaTwo);
        CHECK(lambdaOne != firstAdd);
    }

    TEST_CASE("delegate - copies invoke the same target") {
        counter instance = {};
        const delegate<void(int)> original = delegate<void(int)>::bind<&counter::add>(&instance);
        delegate<void(int)> copy = original;
        delegate<void(int)> assigned = {};
        assigned = original;

        copy(1);
        assigned(2);
        CHECK(instance.total() == 3);
        CHECK(copy == original);
        CHECK(assigned == original);
    }

    TEST_CASE("delegate - a delegate can be rebound to a different kind of target") {
        counter instance(100);
        delegate<int(int)> target = delegate<int(int)>::bind<&add_one>();
        CHECK(target(1) == 2);

        target = delegate<int(int)>::bind<&counter::scaled>(&instance);
        CHECK(target(2) == 200);

        target = delegate<int(int)>::bind([](int value) {
            return -value;
        });
        CHECK(target(3) == -3);
    }
}
