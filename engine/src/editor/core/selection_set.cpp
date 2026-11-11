#include "conjure/editor/selection_set.h"

#include <algorithm>

namespace conjure::editor {

    selection_set::selection_set() = default;
    selection_set::~selection_set() = default;

    void selection_set::set(std::span<const core::uuid> guids) {
        std::vector<core::uuid> next = {};
        next.reserve(guids.size());
        for (const core::uuid &guid : guids) {
            if (!guid.is_null()) {
                next.push_back(guid);
            }
        }
        std::sort(next.begin(), next.end());
        next.erase(std::unique(next.begin(), next.end()), next.end());
        if (next == mGuids) {
            return;
        }
        mGuids = std::move(next);
        mByEntity.clear();
        mOnChanged.emit();
    }

    void selection_set::add(std::span<const core::uuid> guids) {
        bool changed = false;
        for (const core::uuid &guid : guids) {
            if (guid.is_null()) {
                continue;
            }
            const auto position = std::lower_bound(mGuids.begin(), mGuids.end(), guid);
            if (position != mGuids.end() && *position == guid) {
                continue;
            }
            mGuids.insert(position, guid);
            changed = true;
        }
        if (changed) {
            mByEntity.clear();
            mOnChanged.emit();
        }
    }

    void selection_set::remove(std::span<const core::uuid> guids) {
        bool changed = false;
        for (const core::uuid &guid : guids) {
            const auto position = std::lower_bound(mGuids.begin(), mGuids.end(), guid);
            if (position == mGuids.end() || *position != guid) {
                continue;
            }
            mGuids.erase(position);
            changed = true;
        }
        if (changed) {
            mByEntity.clear();
            mOnChanged.emit();
        }
    }

    void selection_set::clear() {
        if (mGuids.empty()) {
            return;
        }
        mGuids.clear();
        mByEntity.clear();
        mOnChanged.emit();
    }

    void selection_set::rebuild(const scene::scene &target) {
        mByEntity.clear();
        bool dropped = false;
        for (usize index = 0; index < mGuids.size();) {
            const ecs::entity entity = target.find_guid(mGuids[index]);
            if (entity.is_null() || !target.contains(entity)) {
                mGuids.erase(mGuids.begin() + static_cast<isize>(index));
                dropped = true;
                continue;
            }
            if (mByEntity.size() <= entity.index) {
                mByEntity.resize(static_cast<usize>(entity.index) + 1, false);
            }
            mByEntity.set(entity.index);
            ++index;
        }
        if (dropped) {
            mOnChanged.emit();
        }
    }

    bool selection_set::contains(const core::uuid &guid) const noexcept {
        return std::binary_search(mGuids.begin(), mGuids.end(), guid);
    }

    bool selection_set::contains(ecs::entity entity) const noexcept {
        if (entity.is_null() || entity.index >= mByEntity.size()) {
            return false;
        }
        return mByEntity.test(entity.index);
    }

    core::uuid selection_set::primary() const noexcept {
        if (mGuids.empty()) {
            return core::uuid::null();
        }
        return mGuids.front();
    }
}
