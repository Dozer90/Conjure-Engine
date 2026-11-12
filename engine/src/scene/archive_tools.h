#pragma once

#include "conjure/scene/components.h"

#include "conjure/assets/asset_types.h"
#include "conjure/core/error.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/entity_archive.h"

#include <cstddef>
#include <cstring>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

// Helpers over ecs::entity_archive shared by the serializer, the prefab functions, the snapshot and the
// link pass: typed reads of trivially copyable components, deep cloning with per-entry edits (the archive
// API itself is append-only), the hierarchy shape of an archive and the asset references it names.

namespace conjure::scene::detail {

    /// <summary>
    /// Index meaning "no archive-local parent".
    /// </summary>
    inline constexpr u32 NO_ENTRY = 0xFFFFFFFFu;

    /// <summary>
    /// Id of ecs::unresolved_components: every component_registry registers it first and asserts id 0
    /// (ecs/component_registry.h). It is not a named component, so registry.id_of cannot name it.
    /// </summary>
    inline constexpr ecs::component_id UNRESOLVED_COMPONENTS_ID{0};

    /// <summary>
    /// The trivially copyable component of one entry, when present.
    /// </summary>
    template <typename ComponentType>
    [[nodiscard]] std::optional<ComponentType> read_component(
      const ecs::entity_archive &archive,
      u32 entryIndex,
      ecs::component_id id
    ) noexcept {
        static_assert(std::is_trivially_copyable_v<ComponentType>, "read_component copies bytes");
        const std::span<const std::byte> bytes = archive.component_bytes(entryIndex, id);
        if (bytes.size() != sizeof(ComponentType)) {
            return std::nullopt;
        }
        ComponentType value = {};
        std::memcpy(&value, bytes.data(), sizeof(ComponentType));
        return value;
    }

    /// <summary>
    /// The guid of an entry; nullopt when it has none or the value is null.
    /// </summary>
    [[nodiscard]] std::optional<core::uuid> entry_guid(
      const ecs::entity_archive &archive,
      u32 entryIndex,
      const scene_component_ids &ids
    ) noexcept;

    /// <summary>
    /// An object image of one component (ops.copy or memcpy of the source bytes), destroyed with the
    /// registered destructor. Movable, not copyable.
    /// </summary>
    class component_image {
        private:
            const ecs::component_info *mInfo = nullptr;
            std::vector<std::byte> mBytes = {};

        public:
            component_image() = default;
            component_image(const ecs::component_info &info, const void *source);
            ~component_image();

            component_image(const component_image &) = delete;
            component_image &operator=(const component_image &) = delete;
            component_image(component_image &&other) noexcept;
            component_image &operator=(component_image &&other) noexcept;

        public:
            [[nodiscard]] std::byte *data() noexcept {
                return mBytes.data();
            }

            [[nodiscard]] const std::byte *data() const noexcept {
                return mBytes.data();
            }

            [[nodiscard]] usize size() const noexcept {
                return mBytes.size();
            }

            /// <summary>
            /// Hands the bytes out without destroying the object (for archive_component storage).
            /// </summary>
            [[nodiscard]] std::vector<std::byte> release() noexcept;

        private:
            void destroy() noexcept;
    };

    /// <summary>
    /// Replaces (or adds) a trivially copyable component in an entry; the vector stays sorted by id.
    /// </summary>
    template <typename ComponentType>
    void set_entry_component(ecs::archive_entry &entry, ecs::component_id id, const ComponentType &value) {
        static_assert(std::is_trivially_copyable_v<ComponentType>, "set_entry_component copies bytes");
        std::vector<std::byte> bytes(sizeof(ComponentType));
        std::memcpy(bytes.data(), &value, sizeof(ComponentType));
        for (ecs::archive_component &component : entry.components) {
            if (component.id == id) {
                component.bytes = std::move(bytes);
                return;
            }
        }
        ecs::archive_component added = {};
        added.id = id;
        added.bytes = std::move(bytes);
        usize position = 0;
        while (position < entry.components.size() && entry.components[position].id.value < id.value) {
            ++position;
        }
        entry.components.insert(entry.components.begin() + static_cast<isize>(position), std::move(added));
        entry.mask.set(id);
    }

    /// <summary>
    /// Removes a component from an entry (destroying its image). Nothing happens when absent.
    /// </summary>
    void remove_entry_component(
      const ecs::component_registry &registry,
      ecs::archive_entry &entry,
      ecs::component_id id
    );

    /// <summary>
    /// Deep-copies an archive entry (ops.copy for non-trivial payloads).
    /// </summary>
    [[nodiscard]] ecs::archive_entry clone_entry(
      const ecs::component_registry &registry,
      const ecs::archive_entry &source
    );

    /// <summary>
    /// Deep-copies an archive, letting 'edit' change every entry (index, entry) before it is appended.
    /// </summary>
    [[nodiscard]] ecs::entity_archive clone_archive(
      const ecs::entity_archive &source,
      const std::function<void(u32, ecs::archive_entry &)> &edit
    );

    /// <summary>
    /// The parent entry of every entry: the archive-local index, or NO_ENTRY for an absent, null or
    /// external parent.
    /// </summary>
    [[nodiscard]] std::vector<u32> archive_parents(const ecs::entity_archive &archive, const scene_component_ids &ids);

    /// <summary>
    /// Indices of the entries without an archive-local parent, in archive order.
    /// </summary>
    [[nodiscard]] std::vector<u32> archive_roots(const ecs::entity_archive &archive, const scene_component_ids &ids);

    /// <summary>
    /// Hierarchy pre-order of an archive (roots in archive order, children in archive order) with the
    /// depth of every entry relative to its root. HIERARCHY_CYCLE when an entry is unreachable.
    /// </summary>
    struct archive_shape {
        std::vector<u32> parents = {};        // archive_parents
        std::vector<u32> order = {};          // pre-order, parents before children
        std::vector<u8> depth = {};           // depth below the archive root of each entry
        std::vector<u32> roots = {};          // entries without a local parent
    };

    [[nodiscard]] core::result<archive_shape> compute_archive_shape(
      const ecs::entity_archive &archive,
      const scene_component_ids &ids
    );

    /// <summary>
    /// DUPLICATE_GUID when two entries of the archive carry the same guid (the KEEP policy's internal
    /// check; collisions with a live scene are the scene's own concern).
    /// </summary>
    [[nodiscard]] core::result<void> check_unique_guids(
      const ecs::entity_archive &archive,
      const scene_component_ids &ids
    );

    /// <summary>
    /// What keeps an archive from having the prefab shape (one root, at entry 0).
    /// </summary>
    enum class prefab_root_problem : u8 {
        NONE,
        EMPTY,                    // no entries at all
        MISPLACED_ROOT,           // a single root that is not entry 0
        ROOT_COUNT                // several roots, or none (a parent cycle)
    };

    /// <summary>
    /// The outcome of inspect_prefab_roots.
    /// </summary>
    struct prefab_root_report {
        prefab_root_problem problem = prefab_root_problem::NONE;
        u32 value = 0;            // the misplaced root's entry, or the number of roots
    };

    /// <summary>
    /// Checks the prefab shape without allocating: entry 0 must be the only entry without an archive-local
    /// parent.
    /// </summary>
    [[nodiscard]] prefab_root_report inspect_prefab_roots(
      const ecs::entity_archive &archive,
      const scene_component_ids &ids
    ) noexcept;

    /// <summary>
    /// The error of a failed inspect_prefab_roots: BAD_FORMAT for an empty archive, PREFAB_MULTIPLE_ROOTS
    /// naming the misplaced root or the root count otherwise. 'what' names the archive in the message.
    /// </summary>
    [[nodiscard]] core::error prefab_roots_error(const prefab_root_report &report, std::string_view what);

    /// <summary>
    /// The prefab shape check of parse_prefab, build_document, instantiate_many and revert. The message
    /// naming the archive is formatted only when the check fails, so a passing check allocates nothing.
    /// </summary>
    template <typename... Args>
    [[nodiscard]] core::result<void> check_prefab_roots(
      const ecs::entity_archive &archive,
      const scene_component_ids &ids,
      std::format_string<Args...> what,
      Args &&...args
    ) {
        const prefab_root_report report = inspect_prefab_roots(archive, ids);
        if (report.problem == prefab_root_problem::NONE) {
            return core::result<void>::success();
        }
        return prefab_roots_error(report, std::format(what, std::forward<Args>(args)...));
    }

    /// <summary>
    /// Everything instantiate_archive rejects before creating anything, evaluated for an archive whose
    /// roots become scene roots of an empty world: validate(), entries without components
    /// (BAD_FORMAT), the hierarchy shape (HIERARCHY_CYCLE, HIERARCHY_TOO_DEEP) and duplicate guids. A
    /// REPLACE load runs this before it unloads the current content, so a bad file leaves it intact.
    /// </summary>
    [[nodiscard]] core::result<void> check_loadable(const ecs::entity_archive &archive, const scene_component_ids &ids);

    /// <summary>
    /// Number of unresolved component entries preserved in the archive (one per unknown name).
    /// </summary>
    [[nodiscard]] u32 count_unresolved(const ecs::entity_archive &archive);

    /// <summary>
    /// Names of every unresolved component in the archive, sorted and deduplicated.
    /// </summary>
    [[nodiscard]] std::vector<std::string> unresolved_names(const ecs::entity_archive &archive);

    /// <summary>
    /// Every non-null asset uuid named by an asset_ref field of the archive, sorted and deduplicated.
    /// </summary>
    [[nodiscard]] std::vector<assets::asset_id> collect_asset_refs(const ecs::entity_archive &archive);

    /// <summary>
    /// A non-owning shared_ptr over an archive the caller keeps alive (snapshots, prefab assets).
    /// </summary>
    [[nodiscard]] std::shared_ptr<const ecs::entity_archive> borrow_archive(
      const ecs::entity_archive &archive
    ) noexcept;
}
