#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"

#include "conjure/core/event.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;

namespace {
    struct counting_target {
        int calls = 0;
        int lastValue = 0;

        void on_value(int value) {
            ++calls;
            lastValue = value;
        }
    };

    // Captured by handlers (through a shared_ptr) so a handler's state owns another subscription.
    struct subscription_holder {
        event_subscription token = {};
    };

    struct event_owner {
        event<> onEmit = {};
    };
}

TEST_SUITE("core/event") {

    TEST_CASE("event - subscribers receive every emitted argument") {
        event<int, std::string_view> onScore = {};
        int receivedPoints = 0;
        std::string receivedName = {};

        const event_subscription token = onScore.subscribe([&](int points, std::string_view name) {
            receivedPoints = points;
            receivedName = std::string(name);
        });

        onScore.emit(10, "player");
        CHECK(receivedPoints == 10);
        CHECK(receivedName == "player");

        onScore.emit(25, "other");
        CHECK(receivedPoints == 25);
        CHECK(receivedName == "other");
    }

    TEST_CASE("event - events without arguments work") {
        event<> onTick = {};
        int ticks = 0;

        const event_subscription token = onTick.subscribe([&ticks] {
            ++ticks;
        });

        onTick.emit();
        onTick.emit();
        CHECK(ticks == 2);
    }

    TEST_CASE("event - events with reference arguments pass the same object to every handler") {
        event<const std::string &> onName = {};
        const std::string *firstSeen = nullptr;
        const std::string *secondSeen = nullptr;

        const event_subscription first = onName.subscribe([&firstSeen](const std::string &name) {
            firstSeen = &name;
        });
        const event_subscription second = onName.subscribe([&secondSeen](const std::string &name) {
            secondSeen = &name;
        });

        const std::string name = "shared";
        onName.emit(name);
        CHECK(firstSeen == &name);
        CHECK(secondSeen == &name);
    }

    TEST_CASE("event - handlers run in subscription order") {
        event<> onEmit = {};
        std::vector<int> order = {};

        const event_subscription first = onEmit.subscribe([&order] {
            order.push_back(1);
        });
        const event_subscription second = onEmit.subscribe([&order] {
            order.push_back(2);
        });
        const event_subscription third = onEmit.subscribe([&order] {
            order.push_back(3);
        });

        onEmit.emit();
        CHECK(order == std::vector<int>{1, 2, 3});
    }

    TEST_CASE("event - subscriber_count tracks live subscriptions") {
        event<int> onValue = {};
        CHECK(onValue.subscriber_count() == 0);

        event_subscription first = onValue.subscribe([](int) {});
        CHECK(onValue.subscriber_count() == 1);

        {
            const event_subscription scoped = onValue.subscribe([](int) {});
            CHECK(onValue.subscriber_count() == 2);
        }
        CHECK(onValue.subscriber_count() == 1);

        first.release();
        CHECK(onValue.subscriber_count() == 0);
    }

    TEST_CASE("event - emitting with no subscribers is a no-op") {
        const event<int> onValue = {};
        onValue.emit(1);
        CHECK(onValue.subscriber_count() == 0);
    }

    TEST_CASE("event - destroying the subscription unsubscribes") {
        event<int> onValue = {};
        int calls = 0;

        {
            const event_subscription token = onValue.subscribe([&calls](int) {
                ++calls;
            });
            onValue.emit(1);
        }
        onValue.emit(2);

        CHECK(calls == 1);
    }

    TEST_CASE("event - release unsubscribes immediately and is idempotent") {
        event<int> onValue = {};
        int calls = 0;
        event_subscription token = onValue.subscribe([&calls](int) {
            ++calls;
        });

        CHECK(token.is_active());
        token.release();
        CHECK_FALSE(token.is_active());
        onValue.emit(1);
        CHECK(calls == 0);

        token.release();
        CHECK_FALSE(token.is_active());
        CHECK(onValue.subscriber_count() == 0);
    }

    TEST_CASE("event - a default constructed subscription is inactive and harmless") {
        event_subscription token = {};
        CHECK_FALSE(token.is_active());
        token.release();
        CHECK_FALSE(token.is_active());
    }

    TEST_CASE("event - moving a subscription transfers ownership without unsubscribing") {
        event<int> onValue = {};
        int calls = 0;
        event_subscription original = onValue.subscribe([&calls](int) {
            ++calls;
        });

        event_subscription moved(std::move(original));
        CHECK_FALSE(original.is_active());
        CHECK(moved.is_active());
        onValue.emit(1);
        CHECK(calls == 1);

        event_subscription assigned = {};
        assigned = std::move(moved);
        CHECK_FALSE(moved.is_active());
        CHECK(assigned.is_active());
        onValue.emit(2);
        CHECK(calls == 2);

        assigned.release();
        onValue.emit(3);
        CHECK(calls == 2);
        CHECK(onValue.subscriber_count() == 0);
    }

    TEST_CASE("event - move assigning over an active subscription releases the old one") {
        event<int> onValue = {};
        int firstCalls = 0;
        int secondCalls = 0;

        event_subscription token = onValue.subscribe([&firstCalls](int) {
            ++firstCalls;
        });
        token = onValue.subscribe([&secondCalls](int) {
            ++secondCalls;
        });

        onValue.emit(1);
        CHECK(firstCalls == 0);
        CHECK(secondCalls == 1);
        CHECK(onValue.subscriber_count() == 1);
    }

    TEST_CASE("event - self move assignment keeps the subscription") {
        event<int> onValue = {};
        int calls = 0;
        event_subscription token = onValue.subscribe([&calls](int) {
            ++calls;
        });

        event_subscription &alias = token;
        token = std::move(alias);

        CHECK(token.is_active());
        onValue.emit(1);
        CHECK(calls == 1);
    }

    TEST_CASE("event - subscriptions outliving the event become inert") {
        event_subscription token = {};
        {
            event<int> onValue = {};
            token = onValue.subscribe([](int) {});
            CHECK(token.is_active());
        }

        CHECK_FALSE(token.is_active());
        token.release();
        CHECK_FALSE(token.is_active());
    }

    TEST_CASE("event - an empty handler asserts and yields an inactive subscription") {
        testing::expect_assert asserts = {};
        event<int> onValue = {};

        const event_subscription token = onValue.subscribe(std::function<void(int)>());

        CHECK(asserts.count() == 1);
        CHECK_FALSE(token.is_active());
        CHECK(onValue.subscriber_count() == 0);
        onValue.emit(1);
    }

    TEST_CASE("event - subscribing during an emit takes effect for the next emit") {
        event<int> onValue = {};
        int lateCalls = 0;
        event_subscription late = {};

        const event_subscription early = onValue.subscribe([&](int) {
            if (!late.is_active()) {
                late = onValue.subscribe([&lateCalls](int) {
                    ++lateCalls;
                });
                CHECK(onValue.subscriber_count() == 2);
            }
        });

        onValue.emit(1);
        CHECK(lateCalls == 0);
        CHECK(late.is_active());
        CHECK(onValue.subscriber_count() == 2);

        onValue.emit(2);
        CHECK(lateCalls == 1);
    }

    TEST_CASE("event - a subscription made and released during the same emit never runs") {
        event<> onEmit = {};
        int calls = 0;

        const event_subscription outer = onEmit.subscribe([&] {
            event_subscription inner = onEmit.subscribe([&calls] {
                ++calls;
            });
            CHECK(inner.is_active());
            inner.release();
            CHECK_FALSE(inner.is_active());
        });

        onEmit.emit();
        onEmit.emit();
        CHECK(calls == 0);
        CHECK(onEmit.subscriber_count() == 1);
    }

    TEST_CASE("event - a handler may unsubscribe itself during the emit") {
        event<int> onValue = {};
        int calls = 0;
        event_subscription self = {};

        self = onValue.subscribe([&](int) {
            ++calls;
            self.release();
        });

        onValue.emit(1);
        onValue.emit(2);
        CHECK(calls == 1);
        CHECK_FALSE(self.is_active());
        CHECK(onValue.subscriber_count() == 0);
    }

    TEST_CASE("event - a handler unsubscribed earlier in the same emit is skipped") {
        event<int> onValue = {};
        int laterCalls = 0;
        event_subscription later = {};

        const event_subscription first = onValue.subscribe([&later](int) {
            later.release();
        });
        later = onValue.subscribe([&laterCalls](int) {
            ++laterCalls;
        });

        onValue.emit(1);
        CHECK(laterCalls == 0);
        CHECK(onValue.subscriber_count() == 1);
    }

    TEST_CASE("event - a handler unsubscribing an earlier handler leaves the order of the rest intact") {
        event<> onEmit = {};
        std::vector<int> order = {};
        event_subscription first = {};

        first = onEmit.subscribe([&order] {
            order.push_back(1);
        });
        const event_subscription second = onEmit.subscribe([&] {
            order.push_back(2);
            first.release();
        });
        const event_subscription third = onEmit.subscribe([&order] {
            order.push_back(3);
        });

        onEmit.emit();
        onEmit.emit();
        CHECK(order == std::vector<int>{1, 2, 3, 2, 3});
    }

    TEST_CASE("event - emit may be re-entered from a handler") {
        event<int> onValue = {};
        std::vector<int> seen = {};

        const event_subscription token = onValue.subscribe([&](int depth) {
            seen.push_back(depth);
            if (depth < 3) {
                onValue.emit(depth + 1);
            }
        });

        onValue.emit(1);
        CHECK(seen == std::vector<int>{1, 2, 3});
    }

    TEST_CASE("event - changes made in a nested emit are applied once the outermost emit finishes") {
        event<int> onValue = {};
        int addedCalls = 0;
        event_subscription added = {};
        std::vector<usize> countsSeen = {};

        const event_subscription outer = onValue.subscribe([&](int depth) {
            if (depth == 1) {
                onValue.emit(2);
                countsSeen.push_back(onValue.subscriber_count());
                return;
            }
            if (depth == 2) {
                added = onValue.subscribe([&addedCalls](int) {
                    ++addedCalls;
                });
            }
        });

        onValue.emit(1);
        CHECK(addedCalls == 0);
        CHECK(countsSeen == std::vector<usize>{2});

        onValue.emit(3);
        CHECK(addedCalls == 1);
    }

    TEST_CASE("event - a handler may destroy the event it is running on") {
        std::optional<event<int>> owner = {};
        owner.emplace();
        int calls = 0;

        event_subscription token = owner->subscribe([&](int) {
            ++calls;
            owner.reset();
        });

        owner->emit(1);
        CHECK(calls == 1);
        CHECK_FALSE(owner.has_value());
        CHECK_FALSE(token.is_active());
        token.release();
    }

    TEST_CASE("event - releasing a handler that owns another subscription to the same event during an emit is safe") {
        // Regression: compaction used to destroy dead handlers in place, so the owning handler's
        // destructor re-entered unsubscribe() while std::erase_if was still running on mSlots.
        event<> onEmit = {};
        int innerCalls = 0;
        int middleCalls = 0;

        std::shared_ptr<subscription_holder> holder = std::make_shared<subscription_holder>();
        holder->token = onEmit.subscribe([&innerCalls] {
            ++innerCalls;
        });
        event_subscription middle = onEmit.subscribe([holder, &middleCalls] {
            ++middleCalls;
        });
        holder.reset();   // the middle handler is now the only owner of the inner subscription

        const event_subscription releaser = onEmit.subscribe([&middle] {
            middle.release();
        });
        REQUIRE(onEmit.subscriber_count() == 3);

        onEmit.emit();
        CHECK(innerCalls == 1);
        CHECK(middleCalls == 1);
        CHECK_FALSE(middle.is_active());
        CHECK(onEmit.subscriber_count() == 1);

        onEmit.emit();
        CHECK(innerCalls == 1);
        CHECK(middleCalls == 1);
    }

    TEST_CASE("event - releasing a handler that owns another subscription to the same event outside an emit is safe") {
        event<> onEmit = {};
        int innerCalls = 0;

        std::shared_ptr<subscription_holder> holder = std::make_shared<subscription_holder>();
        holder->token = onEmit.subscribe([&innerCalls] {
            ++innerCalls;
        });
        event_subscription middle = onEmit.subscribe([holder] {});
        holder.reset();
        const event_subscription last = onEmit.subscribe([] {});
        REQUIRE(onEmit.subscriber_count() == 3);

        middle.release();   // compaction at depth 0: the inner subscription is released from within it
        CHECK(onEmit.subscriber_count() == 1);
        CHECK(last.is_active());

        onEmit.emit();
        CHECK(innerCalls == 0);
    }

    TEST_CASE("event - a subscription made and released during one emit may own another pending subscription") {
        event<> onEmit = {};
        int innerCalls = 0;

        const event_subscription outer = onEmit.subscribe([&] {
            std::shared_ptr<subscription_holder> holder = std::make_shared<subscription_holder>();
            holder->token = onEmit.subscribe([&innerCalls] {
                ++innerCalls;
            });
            event_subscription transient = onEmit.subscribe([holder] {});
            holder.reset();
            CHECK(onEmit.subscriber_count() == 3);

            // Both live in the pending list; erasing the transient one destroys its handler,
            // which releases the inner one from inside the same list.
            transient.release();
            CHECK(onEmit.subscriber_count() == 1);
        });

        onEmit.emit();
        onEmit.emit();
        CHECK(innerCalls == 0);
        CHECK(onEmit.subscriber_count() == 1);
    }

    TEST_CASE("event - releasing a handler that owns the event itself during an emit is safe") {
        std::shared_ptr<event_owner> owner = std::make_shared<event_owner>();
        const std::weak_ptr<event_owner> watcher = owner;
        event<> &signal = owner->onEmit;

        event_subscription keeper = signal.subscribe([owner] {});
        const event_subscription releaser = signal.subscribe([&keeper] {
            keeper.release();
        });
        owner.reset();
        REQUIRE_FALSE(watcher.expired());

        // Compacting destroys the keeper's handler, the owner and the event in one go.
        signal.emit();
        CHECK(watcher.expired());
        CHECK_FALSE(keeper.is_active());
        CHECK_FALSE(releaser.is_active());
    }

    TEST_CASE("event - a handler bound to a member function sees the arguments") {
        event<int> onValue = {};
        counting_target target = {};

        const event_subscription token = onValue.subscribe([&target](int value) {
            target.on_value(value);
        });

        onValue.emit(7);
        CHECK(target.calls == 1);
        CHECK(target.lastValue == 7);
    }

    TEST_CASE("event - moving an event keeps its subscriptions and empties the source") {
        event<int> source = {};
        int calls = 0;
        const event_subscription token = source.subscribe([&calls](int) {
            ++calls;
        });

        event<int> destination(std::move(source));
        CHECK(destination.subscriber_count() == 1);
        CHECK(source.subscriber_count() == 0);
        CHECK(token.is_active());

        destination.emit(1);
        source.emit(2);
        CHECK(calls == 1);

        SUBCASE("a moved-from event can be used again") {
            int lateCalls = 0;
            const event_subscription late = source.subscribe([&lateCalls](int) {
                ++lateCalls;
            });
            source.emit(3);
            CHECK(lateCalls == 1);
            CHECK(source.subscriber_count() == 1);
        }

        SUBCASE("move assignment transfers as well") {
            event<int> assigned = {};
            assigned = std::move(destination);
            assigned.emit(4);
            CHECK(calls == 2);
            CHECK(destination.subscriber_count() == 0);
        }
    }

    TEST_CASE("event - many subscriptions can be added and removed in arbitrary order") {
        event<int> onValue = {};
        std::vector<int> calls(16, 0);
        std::vector<event_subscription> tokens = {};

        for (int index = 0; index < 16; ++index) {
            tokens.push_back(onValue.subscribe([&calls, index](int) {
                ++calls[static_cast<usize>(index)];
            }));
        }
        CHECK(onValue.subscriber_count() == 16);

        for (int index = 0; index < 16; index += 2) {
            tokens[static_cast<usize>(index)].release();
        }
        CHECK(onValue.subscriber_count() == 8);

        onValue.emit(1);
        for (int index = 0; index < 16; ++index) {
            CHECK(calls[static_cast<usize>(index)] == (index % 2 == 0 ? 0 : 1));
        }

        tokens.clear();
        CHECK(onValue.subscriber_count() == 0);
    }
}
