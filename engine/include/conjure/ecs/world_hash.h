#pragma once

#include "conjure/core/api.h"
#include "conjure/core/types.h"

// Deterministic state hash (docs/design/08_testing.md 3.4, 01_ecs.md R32).
//
// Archetypes are visited in ascending order of their component-set hash (so creation order does not
// matter); within an archetype entities follow chunk order. Per entity: FNV-1a 64 over the entity id,
// then for every component of the archetype in id order the component's name hash combined with the
// hash of its bytes (trivially copyable types: the raw bytes, floats as raw bits; non-trivial types:
// the canonical JSON text of the component). Tags contribute their name hash only. Hashes combine
// with core::hash::combine in visiting order. Allocation-free below 64 archetypes for trivial worlds.
//
// What a recorded hash depends on besides gameplay state:
//   - Entity ids are hashed. Ids created through command buffers from parallel grains or from systems
//     that run concurrently are handed out in thread-timing order, so create from EXCLUSIVE or
//     MAIN_THREAD systems (or outside the scheduler) when hashes are compared across runs.
//   - TRANSIENT components are hashed like any other, bookkeeping included (the scene's
//     transform_state carries the propagation run stamp), so compare hashes only at equal frame
//     counts from process start.
//   - A trivially copyable type registered without a reflection description hashes its raw bytes,
//     padding and pointers included; such a type must not hold process-dependent values.
//   - Row order inside an archetype. Entities are hashed in chunk/row order, which follows the
//     creation, destruction and archetype-move history (a removed row is filled by the archetype's
//     last row). Two worlds holding the same entities and bytes but reached through different
//     structural histories can hash differently; compare worlds built by the same sequence of
//     structural calls or loaded from the same file.
//   - A non-trivial component whose serializer fails contributes its name hash and a constant only,
//     so two worlds that differ only in such a component hash equal.

namespace conjure::ecs {

    class world;

    /// <summary>
    /// Hash of every alive entity and component value in the world. Equal worlds (same entities in the
    /// same row order, same bytes) hash equal regardless of archetype creation order; one flipped float
    /// bit changes it. Non-trivial components are serialised into one JSON writer reused for the whole
    /// call, so the writer allocates as it grows to the largest component, not once per component.
    /// An asset reference field of a described type takes part through its uuid alone, never through
    /// its bound slot, so a world hashes the same in every process and whether or not its references
    /// are bound yet.
    /// Reads the world without locking it: call it while no structural change is in flight.
    /// </summary>
    [[nodiscard]] CONJURE_API u64 world_hash(const world &source);
}
