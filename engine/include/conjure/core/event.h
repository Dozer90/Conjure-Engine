#pragma once

#include "conjure/core/assert.h"
#include "conjure/core/types.h"

#include <functional>
#include <memory>
#include <utility>
#include <vector>

// Multicast events with RAII subscriptions.
//
//   event<int, std::string_view> onScore = {};
//   event_subscription token = onScore.subscribe([](int points, std::string_view who) { ... });
//   onScore.emit(10, "player");       // every live subscriber, in subscription order
//   token.release();                  // or let the token go out of scope
//
// Events are main-thread objects: nothing here is synchronised. Emission is reentrancy-safe. A
// handler may subscribe, unsubscribe (itself included), emit the same event again, or destroy the
// event; subscriptions made during an emit are first invoked by the next emit, and a handler
// unsubscribed during an emit is never invoked again, not even later in that same emit. A
// handler's captured state may itself own subscriptions to the same event: unsubscribed handlers
// are destroyed only once the subscriber list is consistent again.

namespace conjure::core {

    namespace detail {
        /// <summary>
        /// What an event_subscription talks to. Events implement it through their shared state so
        /// a subscription outliving its event finds the state gone and becomes inert.
        /// </summary>
        class subscription_target {
            public:
                subscription_target();
                virtual ~subscription_target();

                subscription_target(const subscription_target &) = delete;
                subscription_target &operator=(const subscription_target &) = delete;

            public:
                /// <summary>
                /// Removes the handler registered under the id. Unknown ids are ignored.
                /// </summary>
                virtual void unsubscribe(u64 id) noexcept = 0;

                /// <summary>
                /// True while the handler registered under the id is still registered.
                /// </summary>
                [[nodiscard]] virtual bool is_subscribed(u64 id) const noexcept = 0;
        };
    }

    /// <summary>
    /// Owns one subscription to an event (or an event_bus channel). Move-only; destroying or
    /// releasing it unsubscribes. When the event is destroyed first the subscription becomes inert
    /// and release() is a no-op.
    /// </summary>
    class event_subscription {
        private:
            std::weak_ptr<detail::subscription_target> mTarget = {};
            u64 mId = 0;

        public:
            /// <summary>
            /// An inactive subscription.
            /// </summary>
            event_subscription() noexcept;

            /// <summary>
            /// Binds to a handler id on a target. Used by event implementations.
            /// </summary>
            event_subscription(std::weak_ptr<detail::subscription_target> target, u64 id) noexcept;

            ~event_subscription();

            event_subscription(const event_subscription &) = delete;
            event_subscription &operator=(const event_subscription &) = delete;

            event_subscription(event_subscription &&other) noexcept;
            event_subscription &operator=(event_subscription &&other) noexcept;

        public:
            /// <summary>
            /// Unsubscribes now. Safe to call more than once and on inactive subscriptions.
            /// </summary>
            void release() noexcept;

            /// <summary>
            /// True while the handler is registered on a live event.
            /// </summary>
            [[nodiscard]] bool is_active() const noexcept;
    };

    /// <summary>
    /// A multicast callback list. Handlers run synchronously on the emitting thread in
    /// subscription order. Movable (subscriptions follow the moved state), not copyable, and not
    /// thread-safe.
    /// </summary>
    template <typename... Args>
    class event {
        public:
            using handler_type = std::function<void(Args...)>;

        private:
            struct slot {
                u64 id = 0;
                handler_type handler = {};
                bool alive = false;
            };

            class state final : public detail::subscription_target {
                private:
                    std::vector<slot> mSlots = {};
                    std::vector<slot> mPending = {};   // subscribed during an emit; merged after it
                    u64 mNextId = 1;
                    u32 mEmitDepth = 0;
                    bool mNeedsCompaction = false;

                public:
                    state() = default;
                    ~state() override = default;

                public:
                    u64 add(handler_type handler) {
                        const u64 id = mNextId;
                        ++mNextId;

                        std::vector<slot> &destination = mEmitDepth > 0 ? mPending : mSlots;
                        destination.push_back(slot{id, std::move(handler), true});
                        return id;
                    }

                    template <typename... Params>
                    void emit(Params &...params) {
                        ++mEmitDepth;

                        // mSlots is never resized while an emit is running: additions queue in
                        // mPending and removals only clear the alive flag, so indexing is stable
                        // even when a handler subscribes, unsubscribes or re-enters emit.
                        const usize count = mSlots.size();
                        for (usize index = 0; index < count; ++index) {
                            if (!mSlots[index].alive) {
                                continue;
                            }
                            mSlots[index].handler(params...);
                        }

                        --mEmitDepth;
                        if (mEmitDepth == 0) {
                            finish_emit();
                        }
                    }

                    [[nodiscard]] usize count() const noexcept {
                        usize live = mPending.size();
                        for (const slot &entry : mSlots) {
                            if (entry.alive) {
                                ++live;
                            }
                        }
                        return live;
                    }

                    void unsubscribe(u64 id) noexcept override {
                        for (slot &entry : mSlots) {
                            if (entry.id == id && entry.alive) {
                                entry.alive = false;
                                mNeedsCompaction = true;
                                if (mEmitDepth == 0) {
                                    finish_emit();
                                }
                                return;
                            }
                        }

                        for (usize index = 0; index < mPending.size(); ++index) {
                            if (mPending[index].id == id) {
                                // Take the handler out before erasing: its destructor may own
                                // another subscription to this event and re-enter us.
                                [[maybe_unused]] handler_type doomed = std::move(mPending[index].handler);
                                mPending.erase(mPending.begin() + static_cast<isize>(index));
                                return;
                            }
                        }
                    }

                    [[nodiscard]] bool is_subscribed(u64 id) const noexcept override {
                        for (const slot &entry : mSlots) {
                            if (entry.id == id) {
                                return entry.alive;
                            }
                        }
                        for (const slot &entry : mPending) {
                            if (entry.id == id) {
                                return true;
                            }
                        }
                        return false;
                    }

                private:
                    void finish_emit() {
                        // Dead handlers are moved out before the vectors change and destroyed as
                        // the very last step: a handler's captured state may own another
                        // subscription to this same event, so its destructor re-enters
                        // unsubscribe() (and finish_emit()) and must find mSlots consistent.
                        // Declared first so it is destroyed last; nothing may touch a member
                        // after that point because the destructors may even destroy the event.
                        std::vector<handler_type> graveyard = {};

                        if (mNeedsCompaction) {
                            for (slot &entry : mSlots) {
                                if (!entry.alive) {
                                    graveyard.push_back(std::move(entry.handler));
                                }
                            }
                            std::erase_if(mSlots, [](const slot &entry) {
                                return !entry.alive;
                            });
                            mNeedsCompaction = false;
                        }

                        for (slot &entry : mPending) {
                            mSlots.push_back(std::move(entry));
                        }
                        mPending.clear();
                    }
            };

            std::shared_ptr<state> mState = std::make_shared<state>();

        public:
            event() = default;
            ~event() = default;

            event(const event &) = delete;
            event &operator=(const event &) = delete;

            event(event &&) noexcept = default;
            event &operator=(event &&) noexcept = default;

        public:
            /// <summary>
            /// Registers a handler. The handler runs until the returned subscription is released
            /// or destroyed. An empty handler asserts and yields an inactive subscription.
            /// </summary>
            [[nodiscard]] event_subscription subscribe(handler_type handler) {
                CONJURE_ASSERT(static_cast<bool>(handler), "event::subscribe requires a callable handler");
                if (!handler) {
                    return event_subscription();
                }

                ensure_state();
                const u64 id = mState->add(std::move(handler));
                return event_subscription(mState, id);
            }

            /// <summary>
            /// Invokes every live handler in subscription order. Safe to call re-entrantly from a
            /// handler, and safe even when a handler destroys this event.
            /// </summary>
            void emit(Args... args) const {
                if (mState == nullptr) {
                    return;
                }

                // Keep the state alive for the duration: a handler may destroy the event.
                const std::shared_ptr<state> keepAlive = mState;
                keepAlive->emit(args...);
            }

            /// <summary>
            /// Number of live subscriptions, including ones made during a running emit.
            /// </summary>
            [[nodiscard]] usize subscriber_count() const noexcept {
                if (mState == nullptr) {
                    return 0;
                }
                return mState->count();
            }

        private:
            void ensure_state() {
                // A moved-from event has no state; give it a fresh one on first use.
                if (mState == nullptr) {
                    mState = std::make_shared<state>();
                }
            }
    };
}
