#pragma once

#include "conjure/core/types.h"

// Deterministic state hash (docs/design/08_testing.md 3.4, 01_ecs.md R32).
//
// Archetypes are visited in ascending order of their component-set hash (so creation order does not
// matter); within an archetype entities follow chunk order. Per entity: FNV-1a 64 over the entity id,
// then for every component of the archetype in id order the component's name hash combined with the
// hash of its bytes (trivially copyable types: the raw bytes, floats as raw bits; non-trivial types:
// the canonical JSON text of the component). Tags contribute their name hash only. Hashes combine
// with core::hash::combine in visiting order. Allocation-free below 64 archetypes for trivial worlds.

namespace conjure::ecs {

    class world;

    /// <summary>
    /// Hash of every alive entity and component value in the world. Equal worlds (same entities,
    /// same bytes) hash equal regardless of archetype creation order; one flipped float bit changes it.
    /// Reads the world without locking it: call it while no structural change is in flight.
    /// </summary>
    [[nodiscard]] u64 world_hash(const world &source);
}
