#include "physics/physics_world_internal.h"

#include "conjure/physics/log_category.h"

#include "conjure/core/assert.h"
#include "conjure/core/log.h"
#include "conjure/core/radix_sort.h"

#include <algorithm>
#include <utility>

namespace conjure::physics {

namespace {

    [[nodiscard]] constexpr u64 pair_key(u32 bodyA, u32 bodyB) noexcept {
        return (static_cast<u64>(bodyA) << 32) | static_cast<u64>(bodyB);
    }

    [[nodiscard]] constexpr u64 sub_shape_key(const raw_contact &contact) noexcept {
        return (static_cast<u64>(contact.subShapeA) << 32) | static_cast<u64>(contact.subShapeB);
    }

    [[nodiscard]] constexpr u8 kind_order(contact_kind kind) noexcept {
        return static_cast<u8>(kind);
    }
}

/*===========================================================================*/
/*===========================================================================*/
// physics_world

void physics_world::collect_contact_events() {
    state &s = *mState;
    CONJURE_ASSERT(s.is_owner_thread(), "physics_world::collect_contact_events is owner-thread only");
    if (!s.is_owner_thread()) {
        return;
    }
    s.collect_events();
}

std::span<const contact_event> physics_world::contact_events() const noexcept {
    return mState->events;
}


/*===========================================================================*/
/*===========================================================================*/
// state

void physics_world::state::emit_event(const contact_event &event, u64 key) {
    if (events.size() >= events.capacity()) {
        ++stats.droppedContactEvents;
        if (eventBufferWarnings.should_log()) {
            CONJURE_LOG_WARN(
              LOG_PHYSICS,
              "contact event buffer full ({}); raise physics_settings::maxContactEventsPerStep",
              settings.maxContactEventsPerStep
            );
        }
        return;
    }
    events.push_back(event);
    eventKeys.push_back(key);
}

void physics_world::state::synthesise_exits(const body_record &row) {
    const u32 bodyValue = row.bodyId.GetIndexAndSequenceNumber();
    pairScratch.clear();
    for (const auto &pair : activePairs) {
        const u32 bodyA = static_cast<u32>(pair.first >> 32);
        const u32 bodyB = static_cast<u32>(pair.first & 0xFFFFFFFFu);
        if (bodyA != bodyValue && bodyB != bodyValue) {
            continue;
        }
        pairScratch.push_back(pair.first);
        const body_record *rowA = bodyA == bodyValue ? &row : row_of_body(bodyA);
        const body_record *rowB = bodyB == bodyValue ? &row : row_of_body(bodyB);
        contact_event event = {};
        event.kind = pair.second.trigger ? contact_kind::TRIGGER_EXIT : contact_kind::EXIT;
        if (rowA != nullptr) {
            event.a = rowA->entity;
            event.layerA = rowA->layer;
        }
        if (rowB != nullptr) {
            event.b = rowB->entity;
            event.layerB = rowB->layer;
        }
        if (pendingExits.size() >= pendingExits.capacity()) {
            ++stats.droppedContactEvents;
        } else {
            pendingExits.push_back(event);
            pendingExitKeys.push_back(pair.first);
        }
    }
    for (const u64 key : pairScratch) {
        activePairs.erase(key);
    }
    pairScratch.clear();
}

void physics_world::state::collect_events() {
    const u32 dropped = contacts.dropped_count();
    if (dropped > 0) {
        stats.droppedContactEvents += dropped;
        if (contactBufferWarnings.should_log()) {
            CONJURE_LOG_WARN(
              LOG_PHYSICS,
              "{} contact records dropped; raise physics_settings::maxContactEventsPerStep ({})",
              dropped,
              settings.maxContactEventsPerStep
            );
        }
    }

    // 1. Concatenate the per-thread buffers (capacity bounded by the global budget).
    rawMerged.clear();
    for (u32 thread = 0; thread < contacts.thread_count(); ++thread) {
        const std::span<const raw_contact> buffer = contacts.buffer(thread);
        for (const raw_contact &contact : buffer) {
            if (rawMerged.size() >= rawMerged.capacity()) {
                ++stats.droppedContactEvents;
                continue;
            }
            rawMerged.push_back(contact);
        }
    }
    const usize count = rawMerged.size();
    if (count > 0) {
        // Two stable passes: by sub-shape pair first, then by body pair, so the run of a body pair
        // has the same inner order whatever thread recorded which callback.
        sortKeys.resize(count);
        sortValues.resize(count);
        sortScratchKeys.resize(count);
        sortScratchValues.resize(count);
        for (usize index = 0; index < count; ++index) {
            sortKeys[index] = sub_shape_key(rawMerged[index]);
            sortValues[index] = static_cast<u32>(index);
        }
        core::radix_sort_u64(sortKeys, sortValues, sortScratchKeys, sortScratchValues);
        for (usize index = 0; index < count; ++index) {
            const raw_contact &contact = rawMerged[sortValues[index]];
            sortKeys[index] = pair_key(contact.bodyA, contact.bodyB);
        }
        core::radix_sort_u64(sortKeys, sortValues, sortScratchKeys, sortScratchValues);

        // 2. Walk runs of equal body pair.
        usize runStart = 0;
        while (runStart < count) {
            const u64 key = sortKeys[runStart];
            usize runEnd = runStart + 1;
            while (runEnd < count && sortKeys[runEnd] == key) {
                ++runEnd;
            }

            u32 added = 0;
            u32 removed = 0;
            const raw_contact *firstAdded = nullptr;
            const raw_contact *firstLive = nullptr;   // first ADDED or PERSISTED record: one with a point
            const raw_contact *firstAny = nullptr;
            f32 impulse = 0.0f;
            u32 points = 0;
            for (usize index = runStart; index < runEnd; ++index) {
                const raw_contact &contact = rawMerged[sortValues[index]];
                if (firstAny == nullptr) {
                    firstAny = &contact;
                }
                switch (contact.kind) {
                    case raw_contact_kind::ADDED:
                        ++added;
                        impulse += contact.impulse;
                        points += contact.pointCount;
                        if (firstAdded == nullptr) {
                            firstAdded = &contact;
                        }
                        if (firstLive == nullptr) {
                            firstLive = &contact;
                        }
                        break;
                    case raw_contact_kind::PERSISTED:
                        points += contact.pointCount;
                        if (firstLive == nullptr) {
                            firstLive = &contact;
                        }
                        break;
                    case raw_contact_kind::REMOVED:
                        ++removed;
                        break;
                }
            }

            const u32 bodyA = static_cast<u32>(key >> 32);
            const u32 bodyB = static_cast<u32>(key & 0xFFFFFFFFu);
            const body_record *rowA = row_of_body(bodyA);
            const body_record *rowB = row_of_body(bodyB);
            const auto known = activePairs.find(key);
            const u32 before = known == activePairs.end() ? 0 : known->second.subShapePairs;
            const bool trigger = known != activePairs.end()
              ? known->second.trigger
              : ((rowA != nullptr && rowA->isTrigger) || (rowB != nullptr && rowB->isTrigger));
            const u32 after = before + added > removed ? before + added - removed : 0;

            if (rowA == nullptr || rowB == nullptr) {
                // A body destroyed between steps got its synthesised exit already (3.3).
                if (known != activePairs.end()) {
                    activePairs.erase(key);
                }
                runStart = runEnd;
                continue;
            }

            contact_event event = {};
            event.a = rowA->entity;
            event.b = rowB->entity;
            event.layerA = rowA->layer;
            event.layerB = rowB->layer;
            // ENTER reports its first new contact (3.6); a STAY reports a live one, never a REMOVED record (whose
            // point and normal are zero), and falls back to the first record only when a full buffer dropped
            // every live one.
            const raw_contact *liveSource = firstLive != nullptr ? firstLive : firstAny;
            const raw_contact *source = firstAdded != nullptr ? firstAdded : liveSource;
            if (before == 0 && after > 0) {
                event.kind = trigger ? contact_kind::TRIGGER_ENTER : contact_kind::ENTER;
                event.point = source->point;
                event.normal = source->normal;
                event.impulse = trigger ? 0.0f : impulse;
                event.contactCount = static_cast<u8>(std::min<u32>(points, 255));
                emit_event(event, key);
            } else if (before > 0 && after > 0 && !trigger) {
                event.kind = contact_kind::STAY;
                event.point = source->point;
                event.normal = source->normal;
                event.contactCount = static_cast<u8>(std::min<u32>(points, 255));
                emit_event(event, key);
            } else if (before == 0 && after == 0 && added > 0) {
                // Entered and left within the step: both events, enter first.
                event.kind = trigger ? contact_kind::TRIGGER_ENTER : contact_kind::ENTER;
                event.point = source->point;
                event.normal = source->normal;
                event.impulse = trigger ? 0.0f : impulse;
                event.contactCount = static_cast<u8>(std::min<u32>(points, 255));
                emit_event(event, key);
            }
            if (after == 0 && (before > 0 || added > 0)) {
                contact_event exit = {};
                exit.a = rowA->entity;
                exit.b = rowB->entity;
                exit.layerA = rowA->layer;
                exit.layerB = rowB->layer;
                exit.kind = trigger ? contact_kind::TRIGGER_EXIT : contact_kind::EXIT;
                emit_event(exit, key);
            }

            if (after == 0) {
                if (known != activePairs.end()) {
                    activePairs.erase(key);
                }
            } else if (known != activePairs.end()) {
                known->second.subShapePairs = static_cast<u16>(std::min<u32>(after, 0xFFFFu));
            } else if (activePairs.size() < settings.maxContactEventsPerStep) {
                contact_pair_state pairState = {};
                pairState.subShapePairs = static_cast<u16>(std::min<u32>(after, 0xFFFFu));
                pairState.trigger = trigger;
                activePairs.try_emplace(key, pairState);
            } else {
                ++stats.droppedContactEvents;
            }
            runStart = runEnd;
        }
    }

    // 3. Done with the raw buffers. Exits synthesised for bodies destroyed since the previous collect
    // join this step's events and the whole span is sorted once more.
    contacts.clear();
    if (!pendingExits.empty()) {
        for (usize index = 0; index < pendingExits.size(); ++index) {
            emit_event(pendingExits[index], pendingExitKeys[index]);
        }
        pendingExits.clear();
        pendingExitKeys.clear();
        sort_events();
    }
    stats.contactEvents = static_cast<u32>(events.size());
}

void physics_world::state::sort_events() {
    const usize count = events.size();
    if (count < 2) {
        return;
    }
    sortKeys.resize(count);
    sortValues.resize(count);
    sortScratchKeys.resize(count);
    sortScratchValues.resize(count);
    // Kind order first (stable), then the body pair, so equal pairs keep enter before exit.
    for (usize index = 0; index < count; ++index) {
        sortKeys[index] = kind_order(events[index].kind);
        sortValues[index] = static_cast<u32>(index);
    }
    core::radix_sort_u64(sortKeys, sortValues, sortScratchKeys, sortScratchValues);
    for (usize index = 0; index < count; ++index) {
        sortKeys[index] = eventKeys[sortValues[index]];
    }
    core::radix_sort_u64(sortKeys, sortValues, sortScratchKeys, sortScratchValues);

    eventScratch.resize(count);
    for (usize index = 0; index < count; ++index) {
        eventScratch[index] = events[sortValues[index]];
    }
    events.swap(eventScratch);
    for (usize index = 0; index < count; ++index) {
        eventKeys[index] = sortKeys[index];
    }
    eventScratch.clear();
}
}
