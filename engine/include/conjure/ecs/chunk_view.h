#pragma once

#include "conjure/ecs/archetype.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"

#include "conjure/core/api.h"
#include "conjure/core/assert.h"
#include "conjure/core/types.h"

#include <span>
#include <type_traits>

// Untyped per-chunk access (docs/design/01_ecs.md 2.4, 4.4). A chunk_view is what queries hand to
// callbacks and what the C# Span<T> is built on. Acquiring a non-const span stamps the column with the
// world version the view was created with (chunk-granular change detection, 3.3); const spans never
// stamp. Inside a system (a view created through system_context) a non-const span of a component the
// system did not declare as written asserts in Debug and Development.

namespace conjure::ecs {

    class chunk;
    class system_context;

    namespace detail {
        /// <summary>
        /// True when the system behind 'context' declared 'id' as written.
        /// </summary>
        [[nodiscard]] CONJURE_API bool context_declares_write(const system_context &context, component_id id) noexcept;

        /// <summary>
        /// Asserts (naming the system and component) about an undeclared write.
        /// </summary>
        CONJURE_API void report_undeclared_write(const system_context &context, component_id id);
    }

    class CONJURE_API chunk_view {
        private:
            archetype *mArchetype = nullptr;
            chunk *mChunk = nullptr;
            const component_registry *mRegistry = nullptr;
            u64 mStampVersion = 0;
            const system_context *mContext = nullptr;
            u32 mChunkIndex = 0;

        public:
            /// <summary>
            /// An invalid view: count() is 0 and every accessor returns empty.
            /// </summary>
            chunk_view() noexcept = default;

            /// <summary>
            /// A view of one chunk. Built by queries and the world; 'context' is the running system when
            /// the view was created through one (Debug access validation).
            /// </summary>
            chunk_view(
              archetype &owner,
              chunk &target,
              u32 chunkIndex,
              const component_registry &registry,
              u64 stampVersion,
              const system_context *context
            ) noexcept;

            ~chunk_view() = default;

            chunk_view(const chunk_view &) noexcept = default;
            chunk_view &operator=(const chunk_view &) noexcept = default;

        public:
            /// <summary>
            /// False for a default-constructed view.
            /// </summary>
            [[nodiscard]] bool is_valid() const noexcept {
                return mChunk != nullptr;
            }

            /// <summary>
            /// Rows in the chunk.
            /// </summary>
            [[nodiscard]] u32 count() const noexcept;

            /// <summary>
            /// The entity of every row.
            /// </summary>
            [[nodiscard]] std::span<const entity> entities() const noexcept;

            /// <summary>
            /// The archetype this chunk belongs to (nullptr for an invalid view).
            /// </summary>
            [[nodiscard]] const archetype *owner() const noexcept {
                return mArchetype;
            }

            /// <summary>
            /// Index of this chunk in its archetype.
            /// </summary>
            [[nodiscard]] u32 chunk_index() const noexcept {
                return mChunkIndex;
            }

            /// <summary>
            /// The version non-const access stamps.
            /// </summary>
            [[nodiscard]] u64 stamp_version() const noexcept {
                return mStampVersion;
            }

            /// <summary>
            /// The column of a required component: a non-const T stamps the column version once, a
            /// const T never does. A component the archetype lacks (or a tag) asserts and yields an
            /// empty span; use has() or get_optional() for optional components.
            /// </summary>
            template <typename ComponentType>
            [[nodiscard]] std::span<ComponentType> get() noexcept {
                using plain_type = std::remove_cv_t<ComponentType>;
                const component_id id = mRegistry == nullptr ? component_id{} : mRegistry->id_of<plain_type>();
                void *data = column_data(id, !std::is_const_v<ComponentType>);
                if (data == nullptr) {
                    CONJURE_ASSERT(false, "chunk_view::get: the archetype has no column for this component");
                    return {};
                }
                return std::span<ComponentType>(static_cast<ComponentType *>(data), count());
            }

            /// <summary>
            /// Like get(), but an absent component yields an empty span without asserting.
            /// </summary>
            template <typename ComponentType>
            [[nodiscard]] std::span<ComponentType> get_optional() noexcept {
                using plain_type = std::remove_cv_t<ComponentType>;
                const component_id id = mRegistry == nullptr ? component_id{} : mRegistry->id_of<plain_type>();
                void *data = column_data(id, !std::is_const_v<ComponentType>);
                if (data == nullptr) {
                    return {};
                }
                return std::span<ComponentType>(static_cast<ComponentType *>(data), count());
            }

            /// <summary>
            /// True when the archetype holds 'id' (tags included).
            /// </summary>
            [[nodiscard]] bool has(component_id id) const noexcept;

            /// <summary>
            /// Start of a component column, or nullptr for a tag or an absent component. forWrite
            /// stamps the column and, inside a system, checks the declared writes.
            /// </summary>
            [[nodiscard]] void *column_data(component_id id, bool forWrite) noexcept;

            /// <summary>
            /// Read-only start of a component column, or nullptr. Never stamps.
            /// </summary>
            [[nodiscard]] const void *column_data(component_id id) const noexcept;

            /// <summary>
            /// Bytes between two rows of a column (the component size); 0 when absent.
            /// </summary>
            [[nodiscard]] u32 column_stride(component_id id) const noexcept;

            /// <summary>
            /// Version last stamped on a column; 0 when absent.
            /// </summary>
            [[nodiscard]] u64 column_version(component_id id) const noexcept;
    };
}
