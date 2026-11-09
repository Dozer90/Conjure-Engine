#pragma once

#include "conjure/ecs/component.h"
#include "conjure/ecs/entity.h"

#include "conjure/core/containers/small_vector.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <array>
#include <memory>
#include <span>
#include <vector>

// Archetypes and chunk layout (docs/design/01_ecs.md 2.3).
//
// Every live entity lives in exactly one archetype (the set of its component types, the empty set
// included). An archetype stores its rows in fixed 16 KiB SoA chunks: the entity column first, then
// one column per non-tag component ordered by descending alignment. Rows are packed: only the last
// chunk may be partial. The world is the sole mutator; this header exposes the read-only view that
// queries, tests, the editor and the hash use.

namespace conjure::ecs {

    class component_registry;
    class chunk;
    class chunk_pool;
    class chunk_view;
    class query;
    class system_context;
    class world;

    /// <summary>
    /// Bytes of one chunk block.
    /// </summary>
    inline constexpr u32 CHUNK_SIZE = 16 * 1024;

    /// <summary>
    /// Alignment of every chunk block.
    /// </summary>
    inline constexpr u32 CHUNK_ALIGNMENT = 64;

    /// <summary>
    /// Minimum alignment of a column start inside a chunk (SIMD-friendly loads).
    /// </summary>
    inline constexpr u32 COLUMN_ALIGNMENT = 16;

    /// <summary>
    /// Fewest rows per chunk before compute() logs a warning about a wasteful archetype.
    /// </summary>
    inline constexpr u32 SMALL_CAPACITY_WARNING = 8;

    /// <summary>
    /// Where each column of an archetype lives inside a chunk block. The entity column sits at offset
    /// 0; columns hold the non-tag components only.
    /// </summary>
    struct chunk_layout {
        static constexpr u16 NO_COLUMN = 0xFFFF;

        u32 capacity = 0;                                                      // rows per chunk
        core::small_vector<component_id, 16> components = {};                  // column -> id
        core::small_vector<u32, 16> columnOffsets = {};                        // column -> byte offset in the block
        core::small_vector<u32, 16> columnSizes = {};                          // column -> bytes per row
        std::array<u16, MAX_COMPONENTS> columnIndex = make_empty_column_index();   // id -> column or NO_COLUMN

        /// <summary>
        /// Computes the maximal layout for a mask: rowBytes = sizeof(entity) + sum of sizes, capacity
        /// starts at CHUNK_SIZE / rowBytes and decrements until every aligned column fits the block.
        /// ARCHETYPE_TOO_LARGE when not even one row fits, UNKNOWN_COMPONENT for an id the registry
        /// does not know. A capacity below SMALL_CAPACITY_WARNING logs one WARN.
        /// </summary>
        [[nodiscard]] static core::result<chunk_layout> compute(
          const component_mask &mask,
          const component_registry &registry
        );

        /// <summary>
        /// Number of data columns (tags excluded).
        /// </summary>
        [[nodiscard]] u32 column_count() const noexcept {
            return static_cast<u32>(components.size());
        }

        /// <summary>
        /// The column of a component id, or NO_COLUMN when the archetype lacks it or it is a tag.
        /// </summary>
        [[nodiscard]] u16 column_of(component_id id) const noexcept {
            if (!id.is_valid()) {
                return NO_COLUMN;
            }
            return columnIndex[id.value];
        }

        /// <summary>
        /// Bytes one row occupies across every column, entity included.
        /// </summary>
        [[nodiscard]] u32 row_bytes() const noexcept {
            u32 total = static_cast<u32>(sizeof(entity));
            for (const u32 size : columnSizes) {
                total += size;
            }
            return total;
        }

        /// <summary>
        /// Byte offset one past the last column (the used part of the block).
        /// </summary>
        [[nodiscard]] u32 block_end() const noexcept;

        /// <summary>
        /// An index table with every id mapped to NO_COLUMN.
        /// </summary>
        [[nodiscard]] static constexpr std::array<u16, MAX_COMPONENTS> make_empty_column_index() noexcept {
            std::array<u16, MAX_COMPONENTS> table = {};
            for (u16 &entry : table) {
                entry = NO_COLUMN;
            }
            return table;
        }
    };

    /// <summary>
    /// One component set and the chunks holding its entities. Read-only outside the world; pointers
    /// to archetypes stay valid for the world's lifetime (the world's list is append-only).
    /// </summary>
    class archetype {
        private:
            friend class world;
            friend class chunk_view;
            friend class query;
            friend class system_context;

            /// <summary>
            /// Graph edge to the archetypes that differ by one component.
            /// </summary>
            struct archetype_edge {
                component_id id = {};
                archetype *withAdded = nullptr;
                archetype *withRemoved = nullptr;
            };

            /// <summary>
            /// Position of one row.
            /// </summary>
            struct row_location {
                u32 chunkIndex = 0;
                u32 row = 0;
            };

            component_mask mMask = {};
            chunk_layout mLayout = {};
            std::vector<std::unique_ptr<chunk>> mChunks = {};
            std::vector<archetype_edge> mEdges = {};
            u32 mEntityCount = 0;
            u32 mIndex = 0;                     // position in the world's archetype list

        public:
            archetype(const component_mask &mask, chunk_layout layout, u32 index);
            ~archetype();

            archetype(const archetype &) = delete;
            archetype &operator=(const archetype &) = delete;
            archetype(archetype &&) = delete;
            archetype &operator=(archetype &&) = delete;

        public:
            /// <summary>
            /// The component set of this archetype.
            /// </summary>
            [[nodiscard]] const component_mask &mask() const noexcept {
                return mMask;
            }

            /// <summary>
            /// The chunk layout shared by every chunk.
            /// </summary>
            [[nodiscard]] const chunk_layout &layout() const noexcept {
                return mLayout;
            }

            /// <summary>
            /// Position in world::archetypes().
            /// </summary>
            [[nodiscard]] u32 index() const noexcept {
                return mIndex;
            }

            /// <summary>
            /// Entities stored here.
            /// </summary>
            [[nodiscard]] u32 entity_count() const noexcept {
                return mEntityCount;
            }

            /// <summary>
            /// Chunks held, including a drained spare kept for reuse.
            /// </summary>
            [[nodiscard]] u32 chunk_count() const noexcept {
                return static_cast<u32>(mChunks.size());
            }

            /// <summary>
            /// True when the set contains 'id'.
            /// </summary>
            [[nodiscard]] bool has(component_id id) const noexcept {
                return mMask.test(id);
            }

            /// <summary>
            /// Rows used in one chunk; 0 for an out-of-range index.
            /// </summary>
            [[nodiscard]] u32 chunk_entity_count(u32 chunkIndex) const noexcept;

            /// <summary>
            /// The entity column of one chunk (count() entries); empty for an out-of-range index.
            /// </summary>
            [[nodiscard]] std::span<const entity> chunk_entities(u32 chunkIndex) const noexcept;

            /// <summary>
            /// Read-only start of a component column in one chunk; nullptr for a tag, an absent
            /// component or an out-of-range chunk.
            /// </summary>
            [[nodiscard]] const void *chunk_column(u32 chunkIndex, component_id id) const noexcept;

            /// <summary>
            /// World version last stamped on a column of one chunk; 0 when absent.
            /// </summary>
            [[nodiscard]] u64 chunk_column_version(u32 chunkIndex, component_id id) const noexcept;

        private:
            [[nodiscard]] chunk *chunk_at(u32 chunkIndex) noexcept;
            [[nodiscard]] const chunk *chunk_at(u32 chunkIndex) const noexcept;
            [[nodiscard]] archetype_edge *find_edge(component_id id) noexcept;
            [[nodiscard]] archetype_edge &edge(component_id id);

            /// <summary>
            /// The chunk the next row goes into (a partial chunk ahead of a drained spare fills before
            /// the spare so only the last used chunk is partial), or nullptr when every chunk is full.
            /// </summary>
            [[nodiscard]] chunk *open_chunk(u32 &chunkIndex) noexcept;

            /// <summary>
            /// Makes sure a chunk with a free row exists, allocating one when needed, without taking
            /// the row: the part of push_row that can fail (ARCHETYPE_TOO_LARGE, OUT_OF_MEMORY), split
            /// off so the world can run observers between the check and the move.
            /// </summary>
            [[nodiscard]] core::result<void> reserve_row(chunk_pool &pool);

            [[nodiscard]] core::result<row_location> push_row(chunk_pool &pool);
    };
}
