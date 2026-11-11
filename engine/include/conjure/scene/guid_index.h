#pragma once

#include "conjure/core/api.h"
#include "conjure/core/containers/flat_hash_map.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/entity.h"

// The guid -> entity index of a scene (docs/design/13_scene.md 3.8). Main thread only; maintained by the
// scene's guid observers and link pass. A collision keeps the first mapping, so duplicates are prevented
// at instantiation (validated against the index) rather than repaired here.

namespace conjure::scene {

    class CONJURE_API guid_index {
        public:
            /// <summary>
            /// Entries reserved by the constructor so small scenes never rehash.
            /// </summary>
            static constexpr u32 INITIAL_CAPACITY = 1024;

        private:
            core::flat_hash_map<core::uuid, ecs::entity> mEntries = {};

        public:
            guid_index();
            ~guid_index();

            guid_index(const guid_index &) = delete;
            guid_index &operator=(const guid_index &) = delete;

        public:
            /// <summary>
            /// The entity mapped to the guid, or NULL_ENTITY.
            /// </summary>
            [[nodiscard]] ecs::entity find(const core::uuid &id) const noexcept;

            /// <summary>
            /// True when the guid is mapped.
            /// </summary>
            [[nodiscard]] bool contains(const core::uuid &id) const noexcept;

            /// <summary>
            /// Number of mappings.
            /// </summary>
            [[nodiscard]] u32 size() const noexcept;

            /// <summary>
            /// Makes room for 'count' mappings so the next insertions never rehash.
            /// </summary>
            void reserve(u32 count);

            /// <summary>
            /// Maps the guid to the entity. Returns false (and keeps the existing mapping) when the guid
            /// is already mapped; a null guid is never inserted.
            /// </summary>
            [[nodiscard]] bool insert(const core::uuid &id, ecs::entity target);

            /// <summary>
            /// Removes the mapping when it points at 'expected'; any other mapping is left alone.
            /// </summary>
            void erase(const core::uuid &id, ecs::entity expected) noexcept;

            /// <summary>
            /// Moves an entity from one guid to another: erases oldId when it maps to the entity and
            /// inserts newId (first mapping wins when newId is taken).
            /// </summary>
            void rebind(ecs::entity target, const core::uuid &oldId, const core::uuid &newId);

            /// <summary>
            /// Drops every mapping; the capacity stays.
            /// </summary>
            void clear() noexcept;

            /// <summary>
            /// Buckets of the underlying table (for the allocation tests).
            /// </summary>
            [[nodiscard]] usize capacity() const noexcept;
    };
}
