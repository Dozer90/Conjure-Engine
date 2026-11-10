#pragma once

#include "physics/jolt_include.h"

#include "conjure/core/containers/flat_hash_map.h"
#include "conjure/core/types.h"

#include <Jolt/Core/Reference.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

// Leaf shapes shared between bodies (docs/design/14_physics.md 3.3): keyed by a 64-bit hash of the
// collider kind, its quantised dimensions and the asset identity; compounds are per entity and never
// cached. collect() drops entries only the cache references.

namespace conjure::physics {

    class shape_cache {
        private:
            core::flat_hash_map<u64, JPH::RefConst<JPH::Shape>> mShapes = {};

        public:
            shape_cache();
            ~shape_cache();

            shape_cache(const shape_cache &) = delete;
            shape_cache &operator=(const shape_cache &) = delete;

        public:
            /// <summary>
            /// Pre-sizes the table.
            /// </summary>
            void reserve(usize count);

            /// <summary>
            /// The cached shape for a key, or nullptr.
            /// </summary>
            [[nodiscard]] JPH::RefConst<JPH::Shape> find(u64 key) const noexcept;

            /// <summary>
            /// Stores a shape under a key (replaces an existing entry).
            /// </summary>
            void insert(u64 key, JPH::RefConst<JPH::Shape> shape);

            /// <summary>
            /// Removes entries whose Jolt reference count is one (the cache's own). Returns how many.
            /// </summary>
            u32 collect();

            /// <summary>
            /// Drops everything.
            /// </summary>
            void clear() noexcept;

            [[nodiscard]] u32 size() const noexcept {
                return static_cast<u32>(mShapes.size());
            }

            /// <summary>
            /// True when every entry is referenced by at least the cache.
            /// </summary>
            [[nodiscard]] bool validate() const noexcept;
    };
}
