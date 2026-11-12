#include "conjure/editor/selection_set.h"

#include <algorithm>

namespace conjure::editor {

    selection_set::selection_set() = default;
    selection_set::~selection_set() = default;

    void selection_set::set(std::span<const core::uuid> guids) {
        std::vector<core::uuid> next = {};
        next.reserve(guids.size());
        core::uuid lastNamed = core::uuid::null();
        for (const core::uuid &guid : guids) {
            if (!guid.is_null()) {
                next.push_back(guid);
                lastNamed = guid;
            }
        }
        std::sort(next.begin(), next.end());
        next.erase(std::unique(next.begin(), next.end()), next.end());
        if (next == mGuids && lastNamed == mPrimary) {
            return;
        }
        mGuids = std::move(next);
        mPrimary = lastNamed;
        mByEntity.clear();
        mOnChanged.emit();
    }

    void selection_set::add(std::span<const core::uuid> guids) {
        bool changed = false;
        core::uuid lastNamed = core::uuid::null();
        for (const core::uuid &guid : guids) {
            if (guid.is_null()) {
                continue;
            }
            lastNamed = guid;
            const auto position = std::lower_bound(mGuids.begin(), mGuids.end(), guid);
            if (position != mGuids.end() && *position == guid) {
                continue;
            }
            mGuids.insert(position, guid);
            changed = true;
        }
        if (!lastNamed.is_null() && lastNamed != mPrimary) {
            mPrimary = lastNamed;
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
            refresh_primary();
            mByEntity.clear();
            mOnChanged.emit();
        }
    }

    void selection_set::clear() {
        if (mGuids.empty()) {
            return;
        }
        mGuids.clear();
        mPrimary = core::uuid::null();
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
            refresh_primary();
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
        return mPrimary;
    }

    void selection_set::refresh_primary() noexcept {
        if (contains(mPrimary)) {
            return;
        }
        mPrimary = mGuids.empty() ? core::uuid::null() : mGuids.front();
    }
}
