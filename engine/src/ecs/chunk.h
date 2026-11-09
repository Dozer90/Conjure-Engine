#pragma once

#include "conjure/ecs/archetype.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/entity.h"

#include "conjure/core/types.h"

#include <atomic>
#include <memory>
#include <span>

// One 16 KiB block of an archetype plus its out-of-line metadata (docs/design/01_ecs.md 2.3). The block
// is pure data: the entity column at offset 0 followed by the component columns at the offsets the
// chunk_layout computed. Column versions are relaxed atomics so concurrent stamping from job threads
// is defined.

namespace conjure::ecs {

    class chunk {
        private:
            void *mBlock = nullptr;                                     // from the world's chunk_pool
            u32 mCount = 0;                                             // rows in use
            u32 mColumnCount = 0;
            std::unique_ptr<std::atomic<u64>[]> mColumnVersions = {};   // one per data column

        public:
            chunk(void *block, u32 columnCount);
            ~chunk();

            chunk(const chunk &) = delete;
            chunk &operator=(const chunk &) = delete;
            chunk(chunk &&) = delete;
            chunk &operator=(chunk &&) = delete;

        public:
            /// <summary>
            /// The raw block. Returned to the pool by the archetype's owner, not by the chunk.
            /// </summary>
            [[nodiscard]] void *block() const noexcept {
                return mBlock;
            }

            /// <summary>
            /// Rows in use.
            /// </summary>
            [[nodiscard]] u32 count() const noexcept {
                return mCount;
            }

            void set_count(u32 count) noexcept {
                mCount = count;
            }

            /// <summary>
            /// The entity column (capacity entries, count() of them live).
            /// </summary>
            [[nodiscard]] entity *entities() noexcept {
                return static_cast<entity *>(mBlock);
            }

            [[nodiscard]] const entity *entities() const noexcept {
                return static_cast<const entity *>(mBlock);
            }

            /// <summary>
            /// The live part of the entity column.
            /// </summary>
            [[nodiscard]] std::span<const entity> live_entities() const noexcept {
                return std::span<const entity>(entities(), mCount);
            }

            /// <summary>
            /// Start of a data column.
            /// </summary>
            [[nodiscard]] void *column(const chunk_layout &layout, u32 columnIndex) noexcept {
                return static_cast<std::byte *>(mBlock) + layout.columnOffsets[columnIndex];
            }

            [[nodiscard]] const void *column(const chunk_layout &layout, u32 columnIndex) const noexcept {
                return static_cast<const std::byte *>(mBlock) + layout.columnOffsets[columnIndex];
            }

            /// <summary>
            /// Address of one row of a data column.
            /// </summary>
            [[nodiscard]] void *cell(const chunk_layout &layout, u32 columnIndex, u32 row) noexcept {
                return static_cast<std::byte *>(column(layout, columnIndex)) +
                  static_cast<usize>(layout.columnSizes[columnIndex]) * row;
            }

            [[nodiscard]] const void *cell(const chunk_layout &layout, u32 columnIndex, u32 row) const noexcept {
                return static_cast<const std::byte *>(column(layout, columnIndex)) +
                  static_cast<usize>(layout.columnSizes[columnIndex]) * row;
            }

            /// <summary>
            /// The version last stamped on a column.
            /// </summary>
            [[nodiscard]] u64 version(u32 columnIndex) const noexcept {
                return mColumnVersions[columnIndex].load(std::memory_order_relaxed);
            }

            /// <summary>
            /// Stamps one column. Relaxed: concurrent stamps of the same chunk are defined.
            /// </summary>
            void stamp(u32 columnIndex, u64 version) noexcept {
                mColumnVersions[columnIndex].store(version, std::memory_order_relaxed);
            }

            /// <summary>
            /// Stamps every column (a row moved in or out of this chunk).
            /// </summary>
            void stamp_all(u64 version) noexcept {
                for (u32 column = 0; column < mColumnCount; ++column) {
                    mColumnVersions[column].store(version, std::memory_order_relaxed);
                }
            }

            /// <summary>
            /// Largest version among the given columns (0 when none has a column).
            /// </summary>
            [[nodiscard]] u64 max_version(const chunk_layout &layout, const component_mask &components) const noexcept;
    };
}
