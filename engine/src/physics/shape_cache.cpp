#include "physics/shape_cache.h"

#include <utility>

namespace conjure::physics {

shape_cache::shape_cache() {}

shape_cache::~shape_cache() {}

void shape_cache::reserve(usize count) {
    mShapes.reserve(count);
}

JPH::RefConst<JPH::Shape> shape_cache::find(u64 key) const noexcept {
    const auto found = mShapes.find(key);
    if (found == mShapes.end()) {
        return nullptr;
    }
    return found->second;
}

void shape_cache::insert(u64 key, JPH::RefConst<JPH::Shape> shape) {
    mShapes.insert_or_assign(key, std::move(shape));
}

u32 shape_cache::collect() {
    u32 removed = 0;
    for (auto iterator = mShapes.begin(); iterator != mShapes.end();) {
        if (iterator->second->GetRefCount() <= 1) {
            iterator = mShapes.erase(iterator);
            ++removed;
            continue;
        }
        ++iterator;
    }
    return removed;
}

void shape_cache::clear() noexcept {
    mShapes.clear();
}

bool shape_cache::validate() const noexcept {
    for (const auto &entry : mShapes) {
        if (entry.second == nullptr || entry.second->GetRefCount() < 1) {
            return false;
        }
    }
    return true;
}
}
