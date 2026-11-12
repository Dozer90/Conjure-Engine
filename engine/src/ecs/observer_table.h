#pragma once

#include "conjure/ecs/component.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/observer.h"

#include "conjure/core/types.h"

#include <array>
#include <deque>
#include <vector>

// Observer registry of one world (docs/design/01_ecs.md 4.6). Slots are generation-checked so a stale
// observer_id never removes a newer observer; unobserve() during a fire only marks the slot and the
// handler is destroyed once the fire has returned. Observers may register observers: the slots live in
// a deque, whose push_back keeps the running handler's slot in place, and a fire never reuses a freed
// slot, so a handler registered during a fire is first invoked by the next one. A per-event interest
// mask (with an observer count per (event, component)) lets a component nobody observes skip the slot
// scan entirely.

namespace conjure::ecs {

    class command_buffer;
    class world;

    class observer_table {
        private:
            /// <summary>
            /// Number of component_event values (REMOVED is the last one).
            /// </summary>
            static constexpr usize EVENT_COUNT = static_cast<usize>(component_event::REMOVED) + 1;

            struct slot {
                u32 generation = 0;
                bool alive = false;
                bool observesEntities = false;      // entity observer (destroyed) vs component observer
                component_id id = {};
                component_event event = component_event::ADDED;
                component_observer_fn componentFn = {};
                entity_observer_fn entityFn = {};
            };

            std::deque<slot> mSlots = {};       // stable addresses across push_back (registration during a fire)
            std::vector<u32> mFree = {};
            std::vector<u32> mPendingRelease = {};
            u32 mNextGeneration = 1;
            u32 mFiringDepth = 0;
            u32 mComponentObserverCount = 0;
            u32 mEntityObserverCount = 0;
            std::array<component_mask, EVENT_COUNT> mObserved = {};                 // per event: ids with an observer
            std::array<u32, EVENT_COUNT * MAX_COMPONENTS> mObserverCounts = {};     // event * MAX_COMPONENTS + id

        public:
            observer_table();
            ~observer_table();

            observer_table(const observer_table &) = delete;
            observer_table &operator=(const observer_table &) = delete;

        public:
            [[nodiscard]] observer_id add_component_observer(
              component_id id,
              component_event event,
              component_observer_fn fn
            );
            [[nodiscard]] observer_id add_entity_observer(entity_observer_fn fn);

            /// <summary>
            /// Removes an observer. Unknown or stale ids are ignored; returns whether one was removed.
            /// </summary>
            bool remove(observer_id id);

            [[nodiscard]] bool has_component_observers() const noexcept {
                return mComponentObserverCount > 0;
            }

            [[nodiscard]] bool has_entity_observers() const noexcept {
                return mEntityObserverCount > 0;
            }

            void fire_component(
              world &owner,
              entity target,
              component_id id,
              component_event event,
              command_buffer &deferred
            );
            void fire_destroyed(world &owner, entity target, command_buffer &deferred);

        private:
            [[nodiscard]] u32 acquire_slot();
            void release_slot(u32 index);
            void finish_fire();
            [[nodiscard]] static usize interest_index(component_id id, component_event event) noexcept;
    };
}
