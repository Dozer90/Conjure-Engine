#pragma once

#include "conjure/core/hash.h"
#include "conjure/core/reflect/field_info.h"
#include "conjure/core/reflect/type_id.h"
#include "conjure/core/types.h"

#include <format>
#include <functional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

// Entities (docs/design/01_ecs.md 2.1).
//
// An entity is an index into the world's record table plus a generation that detects stale handles.
// Generation 0 is never issued, so the all-zero value is NULL_ENTITY and zero-initialised memory holds
// null entities for free. ARCHIVE_LOCAL_GENERATION marks a reference inside an entity_archive that is
// still a local index into the archive and not yet a real entity.

namespace conjure::ecs {

    /// <summary>
    /// Generation that marks an archive-local reference: the index is an entry index inside the
    /// archive. The allocator never issues it.
    /// </summary>
    inline constexpr u32 ARCHIVE_LOCAL_GENERATION = 0xFFFFFFFFu;

    /// <summary>
    /// Handle of one entity: record index plus generation. Trivially copyable, 8 bytes, mirrored by
    /// the C# Entity struct through to_u64()/from_u64(). Alignment is 4 ({u32, u32}); core::reflect caps
    /// a handle field's expected alignment at the owning type's alignment, so a reflected component may
    /// place entity fields at any 4-byte aligned offset without alignas(8).
    /// </summary>
    struct entity {
        u32 index = 0;
        u32 generation = 0;

        /// <summary>
        /// True for NULL_ENTITY (index 0, generation 0).
        /// </summary>
        [[nodiscard]] constexpr bool is_null() const noexcept {
            return index == 0 && generation == 0;
        }

        /// <summary>
        /// True when this handle is a local index inside an entity_archive.
        /// </summary>
        [[nodiscard]] constexpr bool is_archive_local() const noexcept {
            return generation == ARCHIVE_LOCAL_GENERATION;
        }

        /// <summary>
        /// (generation &lt;&lt; 32) | index, the packed form handed to C#.
        /// </summary>
        [[nodiscard]] constexpr u64 to_u64() const noexcept {
            return (static_cast<u64>(generation) << 32) | static_cast<u64>(index);
        }

        /// <summary>
        /// Inverse of to_u64().
        /// </summary>
        [[nodiscard]] static constexpr entity from_u64(u64 packed) noexcept {
            entity value = {};
            value.index = static_cast<u32>(packed & 0xFFFFFFFFu);
            value.generation = static_cast<u32>(packed >> 32);
            return value;
        }

        /// <summary>
        /// An archive-local reference to entry 'localIndex'.
        /// </summary>
        [[nodiscard]] static constexpr entity archive_local(u32 localIndex) noexcept {
            entity value = {};
            value.index = localIndex;
            value.generation = ARCHIVE_LOCAL_GENERATION;
            return value;
        }

        [[nodiscard]] constexpr bool operator==(const entity &other) const noexcept = default;
    };

    static_assert(sizeof(entity) == 8, "entity must be 8 bytes for the C# mirror");
    static_assert(std::is_trivially_copyable_v<entity>, "entity must be trivially copyable");

    /// <summary>
    /// The null entity. Never alive in any world.
    /// </summary>
    inline constexpr entity NULL_ENTITY = {};

    /// <summary>
    /// Maps archive-local references to real entities: entry i of the table is the entity created for
    /// archive entry i. Built once per instantiated copy of an archive and applied to every entity
    /// field of the copy.
    /// </summary>
    class entity_remap {
        private:
            std::vector<entity> mTargets = {};

        public:
            entity_remap() = default;

            /// <summary>
            /// Takes ownership of a local -> real table.
            /// </summary>
            explicit entity_remap(std::vector<entity> targets) :
                mTargets(std::move(targets)) {}

            ~entity_remap() = default;

            entity_remap(const entity_remap &) = default;
            entity_remap &operator=(const entity_remap &) = default;
            entity_remap(entity_remap &&) noexcept = default;
            entity_remap &operator=(entity_remap &&) noexcept = default;

        public:
            /// <summary>
            /// Number of local indices the table covers.
            /// </summary>
            [[nodiscard]] u32 size() const noexcept {
                return static_cast<u32>(mTargets.size());
            }

            /// <summary>
            /// Resizes the table; new entries are NULL_ENTITY.
            /// </summary>
            void resize(u32 count) {
                mTargets.resize(count);
            }

            /// <summary>
            /// Sets the real entity of a local index, growing the table when needed.
            /// </summary>
            void set(u32 localIndex, entity target) {
                if (localIndex >= mTargets.size()) {
                    mTargets.resize(static_cast<usize>(localIndex) + 1);
                }
                mTargets[localIndex] = target;
            }

            /// <summary>
            /// The real entity of a local index, or NULL_ENTITY when out of range.
            /// </summary>
            [[nodiscard]] entity target(u32 localIndex) const noexcept {
                if (localIndex >= mTargets.size()) {
                    return NULL_ENTITY;
                }
                return mTargets[localIndex];
            }

            /// <summary>
            /// Resolves one reference: an archive-local value becomes its real entity (NULL_ENTITY when the
            /// local index is out of range); every other value, real or null, is returned unchanged.
            /// </summary>
            [[nodiscard]] entity resolve(entity value) const noexcept {
                if (!value.is_archive_local()) {
                    return value;
                }
                return target(value.index);
            }

            /// <summary>
            /// The whole table.
            /// </summary>
            [[nodiscard]] std::span<const entity> targets() const noexcept {
                return mTargets;
            }

            /// <summary>
            /// Empties the table.
            /// </summary>
            void clear() noexcept {
                mTargets.clear();
            }
    };
}

template <>
struct std::hash<conjure::ecs::entity> {
    [[nodiscard]] std::size_t operator()(const conjure::ecs::entity &value) const noexcept {
        // Index and generation are both well distributed; one combine step keeps neighbours apart.
        return static_cast<std::size_t>(conjure::core::hash::combine(value.generation, value.index));
    }
};

template <>
struct std::formatter<conjure::ecs::entity> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(const conjure::ecs::entity &value, FormatContext &context) const {
        return std::format_to(context.out(), "{}:{}", value.index, value.generation);
    }
};

// Entity fields of reflected components serialise through the ENTITY kind; the ecs serializer rewrites
// them as archive-local indices (01_ecs.md 3.7).
template <>
struct conjure::core::reflect::reflect_kind_override<conjure::ecs::entity> {
    static constexpr conjure::core::reflect::field_kind KIND = conjure::core::reflect::field_kind::ENTITY;
    static constexpr std::string_view NAME = "conjure.ecs.entity";
};
