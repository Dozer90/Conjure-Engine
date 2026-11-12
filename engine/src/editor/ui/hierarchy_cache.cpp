#include "conjure/editor/ui/hierarchy_cache.h"

#include "conjure/core/string_utils.h"
#include "conjure/scene/hierarchy.h"

#include <algorithm>
#include <optional>
#include <utility>

namespace conjure::editor::ui {

    namespace {
        [[nodiscard]] bool contains_case_insensitive(std::string_view text, std::string_view needle) noexcept {
            if (needle.empty()) {
                return true;
            }
            if (needle.size() > text.size()) {
                return false;
            }
            for (usize start = 0; start + needle.size() <= text.size(); ++start) {
                bool matches = true;
                for (usize index = 0; index < needle.size() && matches; ++index) {
                    matches = core::str::to_lower(text[start + index]) == core::str::to_lower(needle[index]);
                }
                if (matches) {
                    return true;
                }
            }
            return false;
        }
    }

    hierarchy_cache::hierarchy_cache() = default;
    hierarchy_cache::~hierarchy_cache() = default;

    bool hierarchy_cache::refresh_if_dirty(const scene::scene &target, const selection_set &selection) {
        if (!mDirty && mStructureVersion == target.structure_version()) {
            return false;
        }
        rebuild(target, selection);
        return true;
    }

    void hierarchy_cache::set_filter(std::string_view filter) {
        const std::string trimmed = std::string(core::str::trim(filter));
        if (trimmed == mFilter) {
            return;
        }
        mFilter = trimmed;
        mDirty = true;
    }

    void hierarchy_cache::set_expanded(const scene::scene &target, ecs::entity entity, bool expanded) {
        if (entity.is_null()) {
            return;
        }
        const std::optional<core::uuid> guid = target.guid_of(entity);
        if (!guid.has_value()) {
            return;
        }
        if (expanded) {
            if (mExpanded.insert(*guid).second) {
                mDirty = true;
            }
            return;
        }
        if (mExpanded.erase(*guid) > 0) {
            mDirty = true;
        }
    }

    bool hierarchy_cache::is_expanded(const scene::scene &target, ecs::entity entity) const {
        if (entity.is_null()) {
            return false;
        }
        const std::optional<core::uuid> guid = target.guid_of(entity);
        return guid.has_value() && mExpanded.contains(*guid);
    }

    void hierarchy_cache::reveal(const scene::scene &target, ecs::entity entity) {
        ecs::entity parent = target.tree().parent_of(entity);
        while (!parent.is_null()) {
            set_expanded(target, parent, true);
            parent = target.tree().parent_of(parent);
        }
    }

    void hierarchy_cache::refresh_labels(const scene::scene &target, usize first, usize last) noexcept {
        const usize end = std::min(last, mRows.size());
        for (usize index = first; index < end; ++index) {
            hierarchy_row &row = mRows[index];
            if (!target.contains(row.entity)) {
                continue;
            }
            row.nameId = target.name_of(row.entity);
            if (target.is_enabled(row.entity)) {
                row.flags &= ~hierarchy_row_flags::DISABLED;
            } else {
                row.flags |= hierarchy_row_flags::DISABLED;
            }
        }
    }

    void hierarchy_cache::rebuild(const scene::scene &target, const selection_set &selection) {
        mRows.clear();
        mStructureVersion = target.structure_version();
        mDirty = false;
        ++mRebuilds;
        const bool filtering = !mFilter.empty();
        if (filtering) {
            compute_matches(target);
        }
        const scene::hierarchy &tree = target.tree();
        const ecs::component_id prefabInstance = target.ids().prefabInstance;

        // Explicit stack of (entity, depth): a pre-order walk without recursion, children in sibling order.
        struct pending {
            ecs::entity entity = {};
            u16 depth = 0;
        };
        std::vector<pending> stack = {};
        stack.reserve(64);
        const std::span<const scene::root_entry> roots = target.roots();
        for (usize index = roots.size(); index > 0; --index) {
            stack.push_back(pending{roots[index - 1].entity, 0});
        }
        while (!stack.empty()) {
            const pending current = stack.back();
            stack.pop_back();
            if (!target.contains(current.entity)) {
                continue;
            }
            if (filtering && !subtree_matches(target, current.entity)) {
                continue;
            }
            const std::span<const ecs::entity> children = tree.children_of(current.entity);
            hierarchy_row row = {};
            row.entity = current.entity;
            row.nameId = target.name_of(current.entity);
            row.depth = current.depth;
            row.flags = hierarchy_row_flags::NONE;
            if (!children.empty()) {
                row.flags |= hierarchy_row_flags::HAS_CHILDREN;
            }
            // A filter shows every matching descendant, so filtered trees draw expanded. The guid lookup runs
            // only for rows with children.
            const bool expanded = filtering || (!children.empty() && is_expanded(target, current.entity));
            if (expanded && !children.empty()) {
                row.flags |= hierarchy_row_flags::EXPANDED;
            }
            if (selection.contains(current.entity)) {
                row.flags |= hierarchy_row_flags::SELECTED;
            }
            if (target.world().has_raw(current.entity, prefabInstance)) {
                row.flags |= hierarchy_row_flags::PREFAB_INSTANCE;
            }
            if (!target.is_enabled(current.entity)) {
                row.flags |= hierarchy_row_flags::DISABLED;
            }
            mRows.push_back(row);
            if (expanded) {
                const u16 childDepth = static_cast<u16>(std::min<u32>(current.depth + 1u, 0xFFFFu));
                for (usize index = children.size(); index > 0; --index) {
                    stack.push_back(pending{children[index - 1], childDepth});
                }
            }
        }
    }

    void hierarchy_cache::compute_matches(const scene::scene &target) {
        mMatches.clear();
        const scene::hierarchy &tree = target.tree();
        // Post-order: an entity matches when its name does or any child does.
        struct pending {
            ecs::entity entity = {};
            bool expanded = false;
        };
        std::vector<pending> stack = {};
        for (const scene::root_entry &root : target.roots()) {
            stack.push_back(pending{root.entity, false});
        }
        while (!stack.empty()) {
            pending &current = stack.back();
            if (!current.expanded) {
                current.expanded = true;
                const std::span<const ecs::entity> children = tree.children_of(current.entity);
                for (usize index = children.size(); index > 0; --index) {
                    stack.push_back(pending{children[index - 1], false});
                }
                continue;
            }
            const ecs::entity entity = current.entity;
            stack.pop_back();
            bool matches = contains_case_insensitive(target.name_of(entity).view(), mFilter);
            if (!matches) {
                for (const ecs::entity child : tree.children_of(entity)) {
                    if (child.index < mMatches.size() && mMatches.test(child.index)) {
                        matches = true;
                        break;
                    }
                }
            }
            if (matches) {
                if (entity.index >= mMatches.size()) {
                    mMatches.resize(static_cast<usize>(entity.index) + 1, false);
                }
                mMatches.set(entity.index);
            }
        }
    }

    bool hierarchy_cache::subtree_matches(const scene::scene &target, ecs::entity entity) const {
        (void) target;
        return entity.index < mMatches.size() && mMatches.test(entity.index);
    }
}
