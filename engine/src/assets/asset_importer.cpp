#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_database.h"
#include "conjure/assets/log_category.h"

#include "conjure/core/assert.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/log.h"
#include "conjure/core/string_utils.h"
#include "conjure/core/vfs/path_utils.h"

#include <algorithm>
#include <format>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

using namespace conjure;
using namespace conjure::assets;

/*===========================================================================*/
/*===========================================================================*/
// sub_asset_table

sub_asset_table::sub_asset_table() {}

sub_asset_table::sub_asset_table(std::vector<sub_asset_entry> existing) :
    mEntries(std::move(existing)) {}

sub_asset_table::~sub_asset_table() {}

asset_id sub_asset_table::id_for(std::string_view subPath) {
    const std::scoped_lock lock(mMutex);
    for (const sub_asset_entry &entry : mEntries) {
        if (entry.subPath == subPath) {
            return entry.id;
        }
    }
    const asset_id allocated = asset_id::generate();
    mEntries.push_back(sub_asset_entry{std::string(subPath), allocated});
    mChanged = true;
    return allocated;
}

std::vector<sub_asset_entry> sub_asset_table::entries() const {
    std::vector<sub_asset_entry> copy = {};
    {
        const std::scoped_lock lock(mMutex);
        copy = mEntries;
    }
    std::sort(copy.begin(), copy.end(), [](const sub_asset_entry &a, const sub_asset_entry &b) {
        return a.subPath < b.subPath;
    });
    return copy;
}

bool sub_asset_table::changed() const {
    const std::scoped_lock lock(mMutex);
    return mChanged;
}

/*===========================================================================*/
/*===========================================================================*/
// import_context

import_context::import_context(
  core::vfs::file_system &fileSystem,
  std::string assetPath,
  std::string filePath,
  std::span<const std::byte> sourceBytes,
  core::json::value settings,
  std::string platform,
  const asset_id &primaryId,
  sub_asset_table &subAssets,
  asset_path_resolver resolver
) :
    mFileSystem(&fileSystem),
    mAssetPath(std::move(assetPath)),
    mFilePath(std::move(filePath)),
    mSourceBytes(sourceBytes),
    mSettings(settings),
    mPlatform(std::move(platform)),
    mPrimaryId(primaryId),
    mSubAssets(&subAssets),
    mResolver(std::move(resolver)) {}

import_context::~import_context() {}

core::result<std::vector<std::byte>> import_context::read_side_file(std::string_view relativePath) {
    const std::string_view directory = core::vfs::path_utils::parent(mFilePath);
    const std::string filePath = core::vfs::path_utils::normalize(core::vfs::path_utils::join(directory, relativePath));

    core::result<std::vector<std::byte>> bytes = mFileSystem->read_file(filePath);
    if (!bytes.has_value()) {
        return make_asset_error(
          asset_error_code::IO_ERROR,
          std::format("cannot read side file '{}' of '{}': {}", relativePath, mAssetPath, bytes.get_error().message())
        );
    }

    // Side files are recorded by asset path so the database can hash and watch them.
    const std::optional<asset_path_parts> parts = split_asset_path(mAssetPath);
    std::string sidePath = filePath;
    if (parts.has_value()) {
        const std::string_view assetDirectory = core::vfs::path_utils::parent(parts->relativePath);
        const std::string relative =
          core::vfs::path_utils::normalize(core::vfs::path_utils::join(assetDirectory, relativePath));
        sidePath = make_asset_path(parts->mount, relative);
    }
    if (std::find(mSideFiles.begin(), mSideFiles.end(), sidePath) == mSideFiles.end()) {
        mSideFiles.push_back(sidePath);
        std::sort(mSideFiles.begin(), mSideFiles.end());
    }
    return bytes;
}

asset_id import_context::sub_asset_id(std::string_view subPath) {
    CONJURE_ASSERT(!subPath.empty(), "sub_asset_id needs a non-empty sub path");
    if (subPath.empty()) {
        return asset_id();
    }
    return mSubAssets->id_for(subPath);
}

std::optional<asset_id> import_context::find_asset_by_path(std::string_view path) const {
    if (!mResolver) {
        return std::nullopt;
    }
    if (path.find("://") != std::string_view::npos) {
        return mResolver(path);
    }
    const std::optional<asset_path_parts> parts = split_asset_path(mAssetPath);
    if (!parts.has_value()) {
        return std::nullopt;
    }
    const std::string_view assetDirectory = core::vfs::path_utils::parent(parts->relativePath);
    const std::string relative = core::vfs::path_utils::normalize(core::vfs::path_utils::join(assetDirectory, path));
    return mResolver(make_asset_path(parts->mount, relative));
}

void import_context::add_warning(std::string message) {
    mWarnings.push_back(std::move(message));
}

/*===========================================================================*/
/*===========================================================================*/
// asset_importer

asset_importer::asset_importer() {}

asset_importer::~asset_importer() {}

core::json::document asset_importer::default_settings() const {
    return core::json::document::object();
}

/*===========================================================================*/
/*===========================================================================*/
// importer_registry

importer_registry::importer_registry() {}

importer_registry::~importer_registry() {}

core::result<void> importer_registry::register_importer(std::unique_ptr<asset_importer> importer) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "importer_registry::register_importer is main-thread only");

    if (importer == nullptr) {
        return core::error(core::error_code::INVALID_ARGUMENT, "importer is null");
    }
    const std::string_view id = importer->id();
    if (id.empty()) {
        return core::error(core::error_code::INVALID_ARGUMENT, "importer has an empty id");
    }
    if (find(id) != nullptr) {
        return core::error(core::error_code::ALREADY_EXISTS, std::format("importer '{}' is already registered", id));
    }

    CONJURE_LOG_DEBUG(LOG_ASSETS, "registered importer '{}' (version {})", id, importer->version());
    mView.push_back(importer.get());
    mImporters.push_back(std::move(importer));
    return core::status::success();
}

core::result<void> importer_registry::unregister_importer(std::string_view id) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "importer_registry::unregister_importer is main-thread only");

    const auto found = std::find_if(mImporters.begin(), mImporters.end(), [id](const auto &candidate) {
        return candidate->id() == id;
    });
    if (found == mImporters.end()) {
        return core::error(core::error_code::NOT_FOUND, std::format("importer '{}' is not registered", id));
    }
    if (imports_in_flight() > 0) {
        return core::error(core::error_code::INVALID_STATE, "imports are in flight");
    }

    mView.erase(mView.begin() + (found - mImporters.begin()));
    mImporters.erase(found);
    return core::status::success();
}

const asset_importer *importer_registry::find(std::string_view id) const noexcept {
    for (const asset_importer *importer : mView) {
        if (importer->id() == id) {
            return importer;
        }
    }
    return nullptr;
}

const asset_importer *importer_registry::find_for_extension(std::string_view extension) const noexcept {
    std::string_view wanted = extension;
    if (!wanted.empty() && wanted.front() == '.') {
        wanted.remove_prefix(1);
    }
    if (wanted.empty()) {
        return nullptr;
    }
    for (const asset_importer *importer : mView) {
        for (const std::string_view candidate : importer->extensions()) {
            std::string_view bare = candidate;
            if (!bare.empty() && bare.front() == '.') {
                bare.remove_prefix(1);
            }
            if (core::str::iequals(bare, wanted)) {
                return importer;
            }
        }
    }
    return nullptr;
}

std::span<const asset_importer *const> importer_registry::importers() const noexcept {
    return mView;
}

void importer_registry::begin_imports() noexcept {
    mImportsInFlight.fetch_add(1, std::memory_order_acq_rel);
}

void importer_registry::end_imports() noexcept {
    CONJURE_ASSERT(imports_in_flight() > 0, "end_imports without begin_imports");
    if (imports_in_flight() == 0) {
        return;
    }
    mImportsInFlight.fetch_sub(1, std::memory_order_acq_rel);
}
