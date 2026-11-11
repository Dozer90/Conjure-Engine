#pragma once

#include "conjure/ecs/component.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/entity.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <span>
#include <vector>

// Entity archives (docs/design/01_ecs.md 2.6, 3.6, 4.8): an in-memory, typed copy of N entities that
// serves prefabs, scene files, copy/paste, undo and the C# Spawn(). Entity fields that point inside
// the captured set hold {localIndex, ARCHIVE_LOCAL_GENERATION}; references outside the set become
// NULL_ENTITY unless archive_options::keepExternalReferences is set. Entry 0 is the root.

namespace conjure::ecs {

    class component_registry;
    class world;

    /// <summary>
    /// One component of one archived entity. The bytes are an object image produced by ops.copy (or
    /// memcpy) and destroyed by the archive; tags have no bytes.
    /// </summary>
    struct archive_component {
        component_id id = {};
        std::vector<std::byte> bytes = {};
    };

    /// <summary>
    /// One archived entity.
    /// </summary>
    struct archive_entry {
        component_mask mask = {};
        std::vector<archive_component> components = {};
    };

    /// <summary>
    /// Capture options.
    /// </summary>
    struct archive_options {
        bool keepExternalReferences = false;    // true for editor copy/paste inside one world
    };

    /// <summary>
    /// A typed copy of a set of entities, bound to the registry whose ids it uses. Move-only; the
    /// destructor destroys non-trivial component payloads.
    /// </summary>
    class CONJURE_API entity_archive {
        private:
            const component_registry *mRegistry = nullptr;
            std::vector<archive_entry> mEntries = {};

        public:
            /// <summary>
            /// An empty archive bound to a registry.
            /// </summary>
            explicit entity_archive(const component_registry &registry) noexcept;

            ~entity_archive();

            entity_archive(const entity_archive &) = delete;
            entity_archive &operator=(const entity_archive &) = delete;

            entity_archive(entity_archive &&other) noexcept;
            entity_archive &operator=(entity_archive &&other) noexcept;

        public:
            /// <summary>
            /// Copies the given entities (every non-TRANSIENT component through ops.copy) and rewrites
            /// their entity fields to local indices. ENTITY_NOT_ALIVE when an entity is dead; the
            /// world must be unlocked (STRUCTURE_LOCKED otherwise).
            /// </summary>
            [[nodiscard]] static core::result<entity_archive> capture(
              const world &source,
              std::span<const entity> entities,
              archive_options options = {}
            );

            /// <summary>
            /// Number of archived entities.
            /// </summary>
            [[nodiscard]] u32 entity_count() const noexcept {
                return static_cast<u32>(mEntries.size());
            }

            /// <summary>
            /// The entries in capture order (entry 0 is the root).
            /// </summary>
            [[nodiscard]] std::span<const archive_entry> entries() const noexcept {
                return mEntries;
            }

            /// <summary>
            /// The registry the component ids belong to.
            /// </summary>
            [[nodiscard]] const component_registry &registry() const noexcept {
                return *mRegistry;
            }

            /// <summary>
            /// Checks that every component id is registered and non-tag payloads have the registered
            /// size, every mask bit has its component (and vice versa), and every archive-local entity
            /// reference points inside the archive. ARCHIVE_CORRUPT otherwise.
            /// </summary>
            [[nodiscard]] core::result<void> validate() const;

            /// <summary>
            /// Appends an entry whose payloads are moved in (used by read_archive_json and tests).
            /// Payload bytes must be object images owned by the caller until this call.
            /// </summary>
            void push_entry(archive_entry entry);

            /// <summary>
            /// The component payload of one entry, or an empty span when absent (or a tag).
            /// </summary>
            [[nodiscard]] std::span<const std::byte> component_bytes(u32 entryIndex, component_id id) const noexcept;

        private:
            void destroy_payloads() noexcept;
    };
}
