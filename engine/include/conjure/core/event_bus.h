#pragma once

#include "conjure/core/api.h"
#include "conjure/core/event.h"
#include "conjure/core/types.h"

#include <functional>
#include <memory>
#include <mutex>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

// Engine-wide typed publish/subscribe.
//
//   struct asset_reloaded { uuid id; };
//   event_subscription token = bus.subscribe<asset_reloaded>([](const asset_reloaded &e) { ... });
//   bus.publish(asset_reloaded{id});          // synchronous, main thread
//   bus.enqueue(asset_reloaded{id});          // from any thread ...
//   bus.dispatch_queued();                    // ... delivered here by the engine each frame
//
// Events are plain copyable structs identified by their type. Only enqueue() and queued_count()
// are thread-safe; subscribing, publishing and dispatching belong to the main thread.

namespace conjure::core {

    class CONJURE_API event_bus {
        private:
            /// <summary>
            /// Type-erased owner of one event type's subscriber list.
            /// </summary>
            class CONJURE_API channel_base {
                public:
                    channel_base();
                    virtual ~channel_base();

                    channel_base(const channel_base &) = delete;
                    channel_base &operator=(const channel_base &) = delete;
            };

            template <typename Event>
            class channel final : public channel_base {
                private:
                    event<const Event &> mSignal = {};

                public:
                    channel() = default;
                    ~channel() override = default;

                public:
                    [[nodiscard]] event<const Event &> &signal() noexcept {
                        return mSignal;
                    }

                    [[nodiscard]] const event<const Event &> &signal() const noexcept {
                        return mSignal;
                    }
            };

            using deferred_publish = std::function<void(event_bus &bus)>;

            std::unordered_map<std::type_index, std::unique_ptr<channel_base>> mChannels = {};
            mutable std::mutex mQueueMutex = {};
            std::vector<deferred_publish> mQueue = {};   // guarded by mQueueMutex

        public:
            event_bus();
            ~event_bus();

            event_bus(const event_bus &) = delete;
            event_bus &operator=(const event_bus &) = delete;
            event_bus(event_bus &&) = delete;
            event_bus &operator=(event_bus &&) = delete;

        public:
            /// <summary>
            /// Registers a handler for one event type. The subscription follows event semantics:
            /// RAII, move-only, inert once the bus is destroyed.
            /// </summary>
            template <typename Event>
            [[nodiscard]] event_subscription subscribe(std::function<void(const Event &)> handler) {
                return channel_for<Event>().signal().subscribe(std::move(handler));
            }

            /// <summary>
            /// Delivers the event to every subscriber of its type now, on the calling thread.
            /// Publishing a type nobody listens to does nothing.
            /// </summary>
            template <typename Event>
            void publish(const Event &eventValue) {
                const channel<Event> *target = find_channel<Event>();
                if (target == nullptr) {
                    return;
                }
                target->signal().emit(eventValue);
            }

            /// <summary>
            /// Stores a copy of the event for the next dispatch_queued(). Thread-safe.
            /// </summary>
            template <typename Event>
            void enqueue(Event eventValue) {
                static_assert(std::is_copy_constructible_v<Event>, "queued events must be copyable");

                deferred_publish deferred = [eventValue = std::move(eventValue)](event_bus &bus) {
                    bus.publish(eventValue);
                };

                const std::scoped_lock lock(mQueueMutex);
                mQueue.push_back(std::move(deferred));
            }

            /// <summary>
            /// Publishes every event enqueued before the call, in enqueue order, and returns how
            /// many were published (events whose type has no subscriber count too). Events
            /// enqueued while dispatching (by handlers or other threads) wait for the next call;
            /// a handler may call this re-entrantly to drain them early. Main thread only.
            /// </summary>
            usize dispatch_queued();

            /// <summary>
            /// Number of events waiting for dispatch_queued(). Thread-safe.
            /// </summary>
            [[nodiscard]] usize queued_count() const;

            /// <summary>
            /// Drops every queued event without publishing it. Thread-safe.
            /// </summary>
            void clear_queue();

            /// <summary>
            /// Number of live subscriptions for one event type.
            /// </summary>
            template <typename Event>
            [[nodiscard]] usize subscriber_count() const noexcept {
                const channel<Event> *target = find_channel<Event>();
                if (target == nullptr) {
                    return 0;
                }
                return target->signal().subscriber_count();
            }

        private:
            template <typename Event>
            [[nodiscard]] channel<Event> &channel_for() {
                static_assert(
                  std::is_same_v<Event, std::remove_cvref_t<Event>>,
                  "event types are identified by their plain type"
                );

                const std::type_index key(typeid(Event));
                auto found = mChannels.find(key);
                if (found == mChannels.end()) {
                    found = mChannels.emplace(key, std::make_unique<channel<Event>>()).first;
                }

                // The key is the exact type, so the stored channel is channel<Event>.
                return static_cast<channel<Event> &>(*found->second);
            }

            template <typename Event>
            [[nodiscard]] const channel<Event> *find_channel() const noexcept {
                // typeid strips qualifiers, so a cv-qualified Event would find the plain channel
                // and then be cast to an unrelated channel type; reject it at compile time.
                static_assert(
                  std::is_same_v<Event, std::remove_cvref_t<Event>>,
                  "event types are identified by their plain type"
                );

                const auto found = mChannels.find(std::type_index(typeid(Event)));
                if (found == mChannels.end()) {
                    return nullptr;
                }
                return static_cast<const channel<Event> *>(found->second.get());
            }
    };
}
