#pragma once

#include "conjure/ecs/archetype.h"
#include "conjure/ecs/chunk_view.h"
#include "conjure/ecs/component.h"

#include "conjure/core/api.h"
#include "conjure/core/containers/small_vector.h"
#include "conjure/core/types.h"

#include <span>
#include <utility>

// Queries (docs/design/01_ecs.md 2.4, 3.2, 4.4). A query is a value type that caches the archetypes
// matching its description and refreshes incrementally (new archetypes only). Iteration locks the
// world for its duration, so structural calls made from a callback fail with STRUCTURE_LOCKED and the
// callback must use a command_buffer instead.

namespace conjure::ecs {

    class component_registry;
    class system_context;
    class world;

    /// <summary>
    /// Matching archetypes a query stores inline before it touches the heap. A view built per system
    /// run (the documented context.view&lt;...&gt;().for_each idiom, system.h) therefore allocates
    /// nothing while it matches at most this many archetypes; beyond that the first refresh of each
    /// new query allocates once.
    /// </summary>
    inline constexpr usize QUERY_INLINE_MATCHES = 16;

    namespace detail {
        /// <summary>
        /// The registry a world was created on; lets headers that cannot include world.h build masks.
        /// </summary>
        [[nodiscard]] CONJURE_API const component_registry &registry_of(const world &target) noexcept;
    }

    /// <summary>
    /// What a query matches: archetypes containing every bit of 'with' and none of 'without'.
    /// 'optional' lists components a view reads when present; 'writes' is the subset of with | optional
    /// the caller intends to write (drives stamping and Debug validation); with a non-empty 'changed'
    /// mask a chunk is visited only when one of the listed columns changed after sinceVersion.
    /// </summary>
    struct query_desc {
        component_mask with = {};
        component_mask without = {};
        component_mask optional = {};
        component_mask writes = {};
        component_mask changed = {};
        u64 sinceVersion = 0;
    };

    class CONJURE_API query {
        private:
            world *mWorld = nullptr;
            const component_registry *mRegistry = nullptr;
            const system_context *mContext = nullptr;       // the running system, for Debug write validation
            query_desc mDesc = {};
            core::small_vector<archetype *, QUERY_INLINE_MATCHES> mMatches = {};
            u32 mSeenArchetypeCount = 0;

        public:
            query(world &target, query_desc desc);
            ~query();

            query(const query &) = default;
            query &operator=(const query &) = default;
            query(query &&) noexcept = default;
            query &operator=(query &&) noexcept = default;

        public:
            /// <summary>
            /// Invokes callable(chunk_view &) for every non-empty chunk of every matching archetype that
            /// passes the changed filter, under an iteration lock. Non-const spans acquired from the
            /// view stamp the world version current when the iteration started.
            /// </summary>
            template <typename Callable>
            void for_each_chunk(Callable &&callable) {
                using callable_type = std::remove_reference_t<Callable>;
                callable_type *target = &callable;
                visit_chunks(
                  [](void *context, chunk_view &view) {
                      (*static_cast<callable_type *>(context))(view);
                  },
                  target
                );
            }

            /// <summary>
            /// Number of chunks the next iteration would visit (refreshes first).
            /// </summary>
            [[nodiscard]] u32 chunk_count();

            /// <summary>
            /// Number of entities in the chunks the next iteration would visit (refreshes first).
            /// </summary>
            [[nodiscard]] u32 entity_count();

            /// <summary>
            /// The chunk at a flat index over the visited chunks (bindings). An out-of-range index yields
            /// an invalid view. The caller holds world::lock_iteration() while using the result.
            /// </summary>
            [[nodiscard]] chunk_view chunk(u32 index);

            /// <summary>
            /// The matching archetypes (refreshes first).
            /// </summary>
            [[nodiscard]] std::span<archetype *const> matches();

            /// <summary>
            /// The description this query was built from.
            /// </summary>
            [[nodiscard]] const query_desc &desc() const noexcept {
                return mDesc;
            }

            /// <summary>
            /// Replaces the description and forgets the cached matches.
            /// </summary>
            void reset(query_desc desc);

            /// <summary>
            /// Scans the archetypes created since the last refresh and caches the matching ones.
            /// </summary>
            void refresh();

            /// <summary>
            /// The world this query iterates.
            /// </summary>
            [[nodiscard]] world &owner() const noexcept {
                return *mWorld;
            }

            /// <summary>
            /// The registry of the world.
            /// </summary>
            [[nodiscard]] const component_registry &registry() const noexcept {
                return *mRegistry;
            }

            /// <summary>
            /// Attaches the running system: every chunk_view this query builds carries it, so a
            /// non-const span of a component the system did not declare as written asserts (Debug
            /// validation, 01_ecs.md 3.5). Set by system_context::view(); nullptr for plain world
            /// queries. The context must outlive the query's iterations.
            /// </summary>
            void bind_context(const system_context *context) noexcept {
                mContext = context;
            }

            /// <summary>
            /// The system bound with bind_context(), or nullptr.
            /// </summary>
            [[nodiscard]] const system_context *context() const noexcept {
                return mContext;
            }

            /// <summary>
            /// True when a chunk of a matching archetype passes the changed filter and is not empty.
            /// </summary>
            [[nodiscard]] bool chunk_passes(const archetype &owner, u32 chunkIndex) const noexcept;

        private:
            void visit_chunks(void (*visit)(void *context, chunk_view &view), void *context);
    };
}
