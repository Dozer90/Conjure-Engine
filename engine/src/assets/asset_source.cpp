#include "conjure/assets/asset_source.h"
#include "conjure/assets/asset_cache.h"
#include "conjure/assets/asset_database.h"
#include "conjure/assets/asset_pack.h"

#include <format>
#include <mutex>
#include <string>
#include <utility>

using namespace conjure;
using namespace conjure::assets;

namespace {
    core::error unknown_asset(const asset_id &id) {
        return make_asset_error(asset_error_code::NOT_FOUND, std::format("asset {} is unknown to the source", id));
    }
}

/*===========================================================================*/
/*===========================================================================*/
// asset_source

asset_source::asset_source() {}

asset_source::~asset_source() {}

/*===========================================================================*/
/*===========================================================================*/
// memory_asset_source

memory_asset_source::memory_asset_source() {}

memory_asset_source::~memory_asset_source() {}

void memory_asset_source::add(cooked_blob blob, std::string_view assetPath) {
    const std::scoped_lock lock(mMutex);
    const asset_id id = blob.header.id;
    entry &stored = mBlobs[id];
    if (!stored.assetPath.empty()) {
        mIdsByPath.erase(stored.assetPath);
    }
    stored.blob = std::move(blob);
    stored.assetPath = std::string(assetPath);
    if (!stored.assetPath.empty()) {
        mIdsByPath.insert_or_assign(stored.assetPath, id);
    }
}

bool memory_asset_source::remove(const asset_id &id) {
    const std::scoped_lock lock(mMutex);
    const auto found = mBlobs.find(id);
    if (found == mBlobs.end()) {
        return false;
    }
    if (!found->second.assetPath.empty()) {
        mIdsByPath.erase(found->second.assetPath);
    }
    mBlobs.erase(found);
    return true;
}

usize memory_asset_source::size() const {
    const std::scoped_lock lock(mMutex);
    return mBlobs.size();
}

bool memory_asset_source::contains(const asset_id &id) const noexcept {
    const std::scoped_lock lock(mMutex);
    return mBlobs.contains(id);
}

std::optional<asset_type_id> memory_asset_source::type_of(const asset_id &id) const noexcept {
    const std::scoped_lock lock(mMutex);
    const auto found = mBlobs.find(id);
    if (found == mBlobs.end()) {
        return std::nullopt;
    }
    return found->second.blob.header.type;
}

std::optional<asset_id> memory_asset_source::find_by_path(std::string_view assetPath) const {
    const std::scoped_lock lock(mMutex);
    const auto found = mIdsByPath.find(std::string(assetPath));
    if (found == mIdsByPath.end()) {
        return std::nullopt;
    }
    return found->second;
}

core::result<cooked_blob> memory_asset_source::read(const asset_id &id) const {
    const std::scoped_lock lock(mMutex);
    const auto found = mBlobs.find(id);
    if (found == mBlobs.end()) {
        return unknown_asset(id);
    }
    return found->second.blob;
}

/*===========================================================================*/
/*===========================================================================*/
// cache_asset_source

cache_asset_source::cache_asset_source(const asset_cache &cache, const asset_database &database) :
    mCache(&cache),
    mDatabase(&database) {}

cache_asset_source::~cache_asset_source() {}

bool cache_asset_source::contains(const asset_id &id) const noexcept {
    return mDatabase->contains(id);
}

std::optional<asset_type_id> cache_asset_source::type_of(const asset_id &id) const noexcept {
    return mDatabase->type_of(id);
}

std::optional<asset_id> cache_asset_source::find_by_path(std::string_view assetPath) const {
    return mDatabase->find_by_path(assetPath);
}

core::result<cooked_blob> cache_asset_source::read(const asset_id &id) const {
    // status_of() copies nothing: this runs on every load job, find() would copy five strings.
    const std::optional<asset_record_status> status = mDatabase->status_of(id);
    if (!status.has_value()) {
        return unknown_asset(id);
    }
    switch (*status) {
        case asset_record_status::META_INVALID:
            return make_asset_error(asset_error_code::META_INVALID, std::format("the meta of {} is invalid", id));
        case asset_record_status::NO_IMPORTER:
            return make_asset_error(asset_error_code::NO_IMPORTER, std::format("no importer for {}", id));
        case asset_record_status::CYCLIC_DEPENDENCY:
            return make_asset_error(
              asset_error_code::CYCLIC_DEPENDENCY,
              std::format("{} is part of a dependency cycle", id)
            );
        case asset_record_status::DUPLICATE_UUID:
            return make_asset_error(
              asset_error_code::DUPLICATE_UUID,
              std::format("{} duplicates another asset's uuid", id)
            );
        case asset_record_status::MISSING_SOURCE:
            return make_asset_error(asset_error_code::NOT_FOUND, std::format("the source of {} is missing", id));
        case asset_record_status::OK:
        case asset_record_status::IMPORT_FAILED:
            break;
    }
    return mCache->read(id);
}

/*===========================================================================*/
/*===========================================================================*/
// pack_asset_source

pack_asset_source::pack_asset_source(const pack_reader &reader) :
    mReader(&reader) {}

pack_asset_source::~pack_asset_source() {}

bool pack_asset_source::contains(const asset_id &id) const noexcept {
    return mReader->find(id) != nullptr;
}

std::optional<asset_type_id> pack_asset_source::type_of(const asset_id &id) const noexcept {
    const pack_toc_entry *entry = mReader->find(id);
    if (entry == nullptr) {
        return std::nullopt;
    }
    return entry->type;
}

std::optional<asset_id> pack_asset_source::find_by_path(std::string_view assetPath) const {
    const pack_toc_entry *entry = mReader->find_by_name(assetPath);
    if (entry == nullptr) {
        return std::nullopt;
    }
    return entry->id;
}

core::result<cooked_blob> pack_asset_source::read(const asset_id &id) const {
    const pack_toc_entry *entry = mReader->find(id);
    if (entry == nullptr) {
        return unknown_asset(id);
    }
    return mReader->read(*entry);
}

/*===========================================================================*/
/*===========================================================================*/
// composite_asset_source

composite_asset_source::composite_asset_source() {}

composite_asset_source::~composite_asset_source() {}

void composite_asset_source::add(std::unique_ptr<asset_source> source) {
    if (source == nullptr) {
        return;
    }
    mSources.push_back(std::move(source));
}

bool composite_asset_source::contains(const asset_id &id) const noexcept {
    for (const auto &source : mSources) {
        if (source->contains(id)) {
            return true;
        }
    }
    return false;
}

std::optional<asset_type_id> composite_asset_source::type_of(const asset_id &id) const noexcept {
    for (const auto &source : mSources) {
        if (source->contains(id)) {
            return source->type_of(id);
        }
    }
    return std::nullopt;
}

std::optional<asset_id> composite_asset_source::find_by_path(std::string_view assetPath) const {
    for (const auto &source : mSources) {
        const std::optional<asset_id> found = source->find_by_path(assetPath);
        if (found.has_value()) {
            return found;
        }
    }
    return std::nullopt;
}

core::result<cooked_blob> composite_asset_source::read(const asset_id &id) const {
    for (const auto &source : mSources) {
        if (source->contains(id)) {
            return source->read(id);
        }
    }
    return unknown_asset(id);
}
