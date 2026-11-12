#include "conjure/testing/doctest.h"

#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/assert_capture.h"

#include "conjure/core/event_bus.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;

namespace {
    struct asset_reloaded {
        int assetId = 0;
    };

    struct window_resized {
        int width = 0;
        int height = 0;
    };

    struct named_event {
        std::string name = {};
    };

    struct subscription_holder {
        event_subscription token = {};
    };
}

TEST_SUITE("core/event_bus") {

    TEST_CASE("event_bus - publish delivers to subscribers of that type only") {
        event_bus bus = {};
        int reloads = 0;
        int resizes = 0;

        const event_subscription onReload = bus.subscribe<asset_reloaded>([&reloads](const asset_reloaded &payload) {
            reloads += payload.assetId;
        });
        const event_subscription onResize = bus.subscribe<window_resized>([&resizes](const window_resized &payload) {
            resizes += payload.width;
        });

        bus.publish(asset_reloaded{3});
        bus.publish(window_resized{1280, 720});
        bus.publish(asset_reloaded{4});

        CHECK(reloads == 7);
        CHECK(resizes == 1280);
    }

    TEST_CASE("event_bus - publishing a type nobody listens to does nothing") {
        event_bus bus = {};
        bus.publish(asset_reloaded{1});
        CHECK(bus.subscriber_count<asset_reloaded>() == 0);
    }

    TEST_CASE("event_bus - publish passes the same object to every subscriber in subscription order") {
        event_bus bus = {};
        std::vector<int> order = {};
        const named_event *firstSeen = nullptr;
        const named_event *secondSeen = nullptr;

        const event_subscription first = bus.subscribe<named_event>([&](const named_event &payload) {
            order.push_back(1);
            firstSeen = &payload;
        });
        const event_subscription second = bus.subscribe<named_event>([&](const named_event &payload) {
            order.push_back(2);
            secondSeen = &payload;
        });

        const named_event payload{"shared"};
        bus.publish(payload);

        CHECK(order == std::vector<int>{1, 2});
        CHECK(firstSeen == &payload);
        CHECK(secondSeen == &payload);
    }

    TEST_CASE("event_bus - subscriber_count is tracked per type") {
        event_bus bus = {};
        CHECK(bus.subscriber_count<asset_reloaded>() == 0);

        event_subscription first = bus.subscribe<asset_reloaded>([](const asset_reloaded &) {});
        const event_subscription second = bus.subscribe<asset_reloaded>([](const asset_reloaded &) {});
        const event_subscription other = bus.subscribe<window_resized>([](const window_resized &) {});

        CHECK(bus.subscriber_count<asset_reloaded>() == 2);
        CHECK(bus.subscriber_count<window_resized>() == 1);
        CHECK(bus.subscriber_count<named_event>() == 0);

        first.release();
        CHECK(bus.subscriber_count<asset_reloaded>() == 1);
    }

    TEST_CASE("event_bus - subscriptions are RAII") {
        event_bus bus = {};
        int calls = 0;

        {
            const event_subscription token = bus.subscribe<asset_reloaded>([&calls](const asset_reloaded &) {
                ++calls;
            });
            bus.publish(asset_reloaded{});
        }
        bus.publish(asset_reloaded{});

        CHECK(calls == 1);
        CHECK(bus.subscriber_count<asset_reloaded>() == 0);
    }

    TEST_CASE("event_bus - subscriptions outliving the bus become inert") {
        event_subscription token = {};
        {
            event_bus bus = {};
            token = bus.subscribe<asset_reloaded>([](const asset_reloaded &) {});
            CHECK(token.is_active());
        }

        CHECK_FALSE(token.is_active());
        token.release();
    }

    TEST_CASE("event_bus - an empty handler asserts and yields an inactive subscription") {
        testing::expect_assert asserts = {};
        event_bus bus = {};

        const event_subscription token = bus.subscribe<asset_reloaded>(std::function<void(const asset_reloaded &)>());

        CHECK(asserts.count() == 1);
        CHECK_FALSE(token.is_active());
        CHECK(bus.subscriber_count<asset_reloaded>() == 0);
    }

    TEST_CASE("event_bus - enqueue defers delivery until dispatch_queued, in enqueue order") {
        event_bus bus = {};
        std::vector<int> received = {};
        const event_subscription token = bus.subscribe<asset_reloaded>([&received](const asset_reloaded &payload) {
            received.push_back(payload.assetId);
        });

        bus.enqueue(asset_reloaded{1});
        bus.enqueue(asset_reloaded{2});
        bus.enqueue(window_resized{10, 20});
        CHECK(received.empty());
        CHECK(bus.queued_count() == 3);

        CHECK(bus.dispatch_queued() == 3);
        CHECK(received == std::vector<int>{1, 2});
        CHECK(bus.queued_count() == 0);

        CHECK(bus.dispatch_queued() == 0);
        CHECK(received == std::vector<int>{1, 2});
    }

    TEST_CASE("event_bus - events queued before a subscriber exists reach a subscriber added before dispatch") {
        event_bus bus = {};
        int calls = 0;

        bus.enqueue(asset_reloaded{5});
        const event_subscription token = bus.subscribe<asset_reloaded>([&calls](const asset_reloaded &payload) {
            calls += payload.assetId;
        });

        CHECK(bus.dispatch_queued() == 1);
        CHECK(calls == 5);
    }

    TEST_CASE("event_bus - queued events keep a copy of the published value") {
        event_bus bus = {};
        std::string received = {};
        const event_subscription token = bus.subscribe<named_event>([&received](const named_event &payload) {
            received = payload.name;
        });

        {
            named_event temporary{"copied"};
            bus.enqueue(temporary);
            temporary.name = "changed after enqueue";
        }

        bus.dispatch_queued();
        CHECK(received == "copied");
    }

    TEST_CASE("event_bus - events enqueued during dispatch wait for the next dispatch") {
        event_bus bus = {};
        std::vector<int> received = {};
        const event_subscription token = bus.subscribe<asset_reloaded>([&](const asset_reloaded &payload) {
            received.push_back(payload.assetId);
            if (payload.assetId < 3) {
                bus.enqueue(asset_reloaded{payload.assetId + 1});
            }
        });

        bus.enqueue(asset_reloaded{1});

        CHECK(bus.dispatch_queued() == 1);
        CHECK(received == std::vector<int>{1});
        CHECK(bus.queued_count() == 1);

        CHECK(bus.dispatch_queued() == 1);
        CHECK(received == std::vector<int>{1, 2});

        CHECK(bus.dispatch_queued() == 1);
        CHECK(bus.dispatch_queued() == 0);
        CHECK(received == std::vector<int>{1, 2, 3});
    }

    TEST_CASE("event_bus - enqueue after dispatch_queued reuses the queue's capacity") {
        event_bus bus = {};
        int reloads = 0;
        const event_subscription onReload = bus.subscribe<asset_reloaded>([&reloads](const asset_reloaded &payload) {
            reloads += payload.assetId;
        });
        bus.enqueue(asset_reloaded{1});
        CHECK(bus.dispatch_queued() == 1);

        // The dispatch hands its emptied buffer back, so the next enqueue finds room without allocating.
        CONJURE_CHECK_NO_ALLOCATIONS(bus.enqueue(asset_reloaded{2}));
        CHECK(bus.dispatch_queued() == 1);
        CHECK(reloads == 3);
    }

    TEST_CASE("event_bus - clear_queue drops queued events without delivering them") {
        event_bus bus = {};
        int calls = 0;
        const event_subscription token = bus.subscribe<asset_reloaded>([&calls](const asset_reloaded &) {
            ++calls;
        });

        bus.enqueue(asset_reloaded{});
        bus.enqueue(asset_reloaded{});
        bus.clear_queue();

        CHECK(bus.queued_count() == 0);
        CHECK(bus.dispatch_queued() == 0);
        CHECK(calls == 0);
    }

    TEST_CASE("event_bus - a handler may publish a different event type synchronously") {
        event_bus bus = {};
        std::vector<std::string> trace = {};

        const event_subscription onReload = bus.subscribe<asset_reloaded>([&](const asset_reloaded &) {
            trace.push_back("reload");
            bus.publish(window_resized{1, 1});
            trace.push_back("after resize");
        });
        const event_subscription onResize = bus.subscribe<window_resized>([&trace](const window_resized &) {
            trace.push_back("resize");
        });

        bus.publish(asset_reloaded{});
        CHECK(trace == std::vector<std::string>{"reload", "resize", "after resize"});
    }

    TEST_CASE("event_bus - a handler may subscribe to a new type while publishing") {
        event_bus bus = {};
        event_subscription late = {};
        int lateCalls = 0;

        const event_subscription onReload = bus.subscribe<asset_reloaded>([&](const asset_reloaded &) {
            if (!late.is_active()) {
                late = bus.subscribe<window_resized>([&lateCalls](const window_resized &) {
                    ++lateCalls;
                });
            }
        });

        bus.publish(asset_reloaded{});
        bus.publish(asset_reloaded{});
        bus.publish(window_resized{});

        CHECK(late.is_active());
        CHECK(lateCalls == 1);
        CHECK(bus.subscriber_count<window_resized>() == 1);
    }

    TEST_CASE("event_bus - a handler may unsubscribe itself while publishing") {
        event_bus bus = {};
        event_subscription self = {};
        int calls = 0;

        self = bus.subscribe<asset_reloaded>([&](const asset_reloaded &) {
            ++calls;
            self.release();
        });

        bus.publish(asset_reloaded{});
        bus.publish(asset_reloaded{});
        CHECK(calls == 1);
        CHECK(bus.subscriber_count<asset_reloaded>() == 0);
    }

    TEST_CASE(
      "event_bus - a handler owning another subscription to the same event type may be released while publishing"
    ) {
        // Regression: bus channels are events, so in-place handler destruction during compaction
        // corrupted the channel's subscriber list when the dying handler owned a subscription.
        event_bus bus = {};
        int innerCalls = 0;

        std::shared_ptr<subscription_holder> holder = std::make_shared<subscription_holder>();
        holder->token = bus.subscribe<asset_reloaded>([&innerCalls](const asset_reloaded &) {
            ++innerCalls;
        });
        event_subscription middle = bus.subscribe<asset_reloaded>([holder](const asset_reloaded &) {});
        holder.reset();
        const event_subscription releaser = bus.subscribe<asset_reloaded>([&middle](const asset_reloaded &) {
            middle.release();
        });
        REQUIRE(bus.subscriber_count<asset_reloaded>() == 3);

        bus.publish(asset_reloaded{1});
        CHECK(innerCalls == 1);
        CHECK_FALSE(middle.is_active());
        CHECK(bus.subscriber_count<asset_reloaded>() == 1);

        bus.publish(asset_reloaded{2});
        CHECK(innerCalls == 1);
    }

    TEST_CASE("event_bus - dispatch_queued re-entered from a handler drains only the events queued so far") {
        event_bus bus = {};
        std::vector<int> received = {};
        usize innerDispatched = 0;

        const event_subscription token = bus.subscribe<asset_reloaded>([&](const asset_reloaded &payload) {
            received.push_back(payload.assetId);
            if (payload.assetId == 1) {
                bus.enqueue(asset_reloaded{3});
                innerDispatched = bus.dispatch_queued();
            }
        });

        bus.enqueue(asset_reloaded{1});
        bus.enqueue(asset_reloaded{2});

        // The outer batch (1, 2) was swapped out before dispatch, so the inner call sees only 3.
        CHECK(bus.dispatch_queued() == 2);
        CHECK(innerDispatched == 1);
        CHECK(received == std::vector<int>{1, 3, 2});
        CHECK(bus.queued_count() == 0);
    }

    TEST_CASE("event_bus - a queued event whose type has no subscriber still counts as dispatched") {
        event_bus bus = {};

        bus.enqueue(window_resized{640, 480});
        CHECK(bus.queued_count() == 1);
        CHECK(bus.subscriber_count<window_resized>() == 0);

        CHECK(bus.dispatch_queued() == 1);
        CHECK(bus.queued_count() == 0);
    }

    TEST_CASE("event_bus - enqueue is safe from many threads at once") {
        event_bus bus = {};
        constexpr int THREAD_COUNT = 8;
        constexpr int PER_THREAD = 2000;

        std::atomic<int> delivered = 0;
        std::vector<int> perThreadCounts(THREAD_COUNT, 0);
        const event_subscription token = bus.subscribe<asset_reloaded>([&](const asset_reloaded &payload) {
            ++delivered;
            ++perThreadCounts[static_cast<usize>(payload.assetId)];
        });

        std::vector<std::thread> producers = {};
        for (int threadIndex = 0; threadIndex < THREAD_COUNT; ++threadIndex) {
            producers.emplace_back([&bus, threadIndex] {
                for (int index = 0; index < PER_THREAD; ++index) {
                    bus.enqueue(asset_reloaded{threadIndex});
                }
            });
        }
        for (std::thread &producer : producers) {
            producer.join();
        }

        CHECK(bus.queued_count() == static_cast<usize>(THREAD_COUNT * PER_THREAD));
        CHECK(bus.dispatch_queued() == static_cast<usize>(THREAD_COUNT * PER_THREAD));
        CHECK(delivered.load() == THREAD_COUNT * PER_THREAD);
        for (const int count : perThreadCounts) {
            CHECK(count == PER_THREAD);
        }
    }

    TEST_CASE("event_bus - enqueue and queued_count stay consistent while the main thread dispatches") {
        event_bus bus = {};
        constexpr int PRODUCER_COUNT = 4;
        constexpr int PER_PRODUCER = 1000;

        int delivered = 0;
        const event_subscription token = bus.subscribe<asset_reloaded>([&delivered](const asset_reloaded &) {
            ++delivered;
        });

        std::atomic<bool> start = false;
        std::vector<std::thread> producers = {};
        for (int threadIndex = 0; threadIndex < PRODUCER_COUNT; ++threadIndex) {
            producers.emplace_back([&bus, &start] {
                while (!start.load()) {
                    std::this_thread::yield();
                }
                for (int index = 0; index < PER_PRODUCER; ++index) {
                    bus.enqueue(asset_reloaded{index});
                }
            });
        }

        // Drain concurrently with the producers; every event must be delivered exactly once.
        start.store(true);
        usize dispatched = 0;
        for (int round = 0; round < 1000; ++round) {
            dispatched += bus.dispatch_queued();
        }
        for (std::thread &producer : producers) {
            producer.join();
        }
        dispatched += bus.dispatch_queued();

        CHECK(dispatched == static_cast<usize>(PRODUCER_COUNT * PER_PRODUCER));
        CHECK(delivered == PRODUCER_COUNT * PER_PRODUCER);
        CHECK(bus.queued_count() == 0);
    }
}
