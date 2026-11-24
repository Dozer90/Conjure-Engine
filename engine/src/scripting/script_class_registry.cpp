#include "conjure/scripting/script_class_registry.h"

#include "conjure/scripting/scripting_error.h"

#include "conjure/core/assert.h"
#include "conjure/core/hash.h"

#include <algorithm>
#include <limits>
#include <utility>

using namespace conjure;
using namespace conjure::scripting;

/*===========================================================================*/
/*===========================================================================*/
// script_class_info

const script_field_info *script_class_info::find_field(u64 fieldHash) const noexcept {
    const auto found = std::lower_bound(
      fields.begin(),
      fields.end(),
      fieldHash,
      [](const script_field_info &field, u64 hash) {
          return field.hash < hash;
      }
    );
    if (found == fields.end() || found->hash != fieldHash) {
        return nullptr;
    }
    return &*found;
}

const script_field_info *script_class_info::find_field(std::string_view fieldName) const noexcept {
    return find_field(script_name_hash(fieldName));
}

void script_class_info::sort_fields() {
    std::sort(fields.begin(), fields.end(), [](const script_field_info &left, const script_field_info &right) {
        return left.hash < right.hash;
    });
}

u64 conjure::scripting::script_name_hash(std::string_view fullName) noexcept {
    return core::hash::fnv1a64(fullName);
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

script_class_registry::script_class_registry() {}

script_class_registry::~script_class_registry() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

void script_class_registry::begin_staging() {
    mStaging = {};
    mStagingOpen = true;
}

core::result<void> script_class_registry::stage_class(script_class_info info) {
    if (!mStagingOpen) {
        return core::error(core::error_code::INVALID_STATE, "stage_class without begin_staging");
    }
    if (mStaging.indexByHash.contains(info.hash)) {
        return core::error(
          core::error_code::ALREADY_EXISTS,
          "script class '" + info.fullName + "' is already staged"
        );
    }
    const u32 index = static_cast<u32>(mStaging.classes.size());
    mStaging.indexByHash.emplace(info.hash, index);
    mStaging.classes.push_back(std::move(info));
    return core::result<void>::success();
}

core::result<void> script_class_registry::stage_field(u64 classHash, script_field_info field) {
    if (!mStagingOpen) {
        return core::error(core::error_code::INVALID_STATE, "stage_field without begin_staging");
    }
    const auto found = mStaging.indexByHash.find(classHash);
    if (found == mStaging.indexByHash.end()) {
        return core::make_error(
          scripting_error::CLASS_NOT_FOUND,
          "stage_field: class {:#018x} is not staged",
          classHash
        );
    }
    script_class_info &owner = mStaging.classes[found->second];
    for (const script_field_info &existing : owner.fields) {
        if (existing.hash == field.hash) {
            return core::error(
              core::error_code::ALREADY_EXISTS,
              "field '" + field.name + "' of '" + owner.fullName + "' is already staged"
            );
        }
    }
    owner.fields.push_back(std::move(field));
    return core::result<void>::success();
}

void script_class_registry::commit_staging() {
    // Without an open staging the staged half is empty: committing it would wipe the active schema.
    CONJURE_ASSERT(mStagingOpen, "script_class_registry::commit_staging without begin_staging");
    if (!mStagingOpen) {
        return;
    }
    for (script_class_info &info : mStaging.classes) {
        info.sort_fields();
    }
    mActive = std::move(mStaging);
    mStaging = {};
    mStagingOpen = false;
}

void script_class_registry::discard_staging() {
    mStaging = {};
    mStagingOpen = false;
}

const script_class_info *script_class_registry::find(u64 classHash) const noexcept {
    return find_in(mActive, classHash);
}

const script_class_info *script_class_registry::find(std::string_view fullName) const noexcept {
    return find_in(mActive, script_name_hash(fullName));
}

const script_class_info *script_class_registry::find_staged(u64 classHash) const noexcept {
    return find_in(mStaging, classHash);
}

u32 script_class_registry::index_of(u64 classHash) const noexcept {
    const auto found = mActive.indexByHash.find(classHash);
    if (found == mActive.indexByHash.end()) {
        return std::numeric_limits<u32>::max();
    }
    return found->second;
}

void script_class_registry::clear() {
    mActive = {};
    mStaging = {};
    mStagingOpen = false;
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

const script_class_info *script_class_registry::find_in(const table &source, u64 classHash) noexcept {
    const auto found = source.indexByHash.find(classHash);
    if (found == source.indexByHash.end()) {
        return nullptr;
    }
    return &source.classes[found->second];
}
