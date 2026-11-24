#include "conjure/assets/asset_cache.h"
#include "conjure/assets/log_category.h"

#include "conjure/core/log.h"
#include "conjure/core/string_utils.h"
#include "conjure/core/vfs/path_utils.h"

#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::assets;

namespace {
    constexpr std::string_view BLOB_EXTENSION = ".cab";
    constexpr std::string_view TEMPORARY_SUFFIX = ".tmp";

    core::error io_error(std::string_view what, std::string_view path, const core::error &cause) {
        return core::make_error(
          assets_error::IO_ERROR,
          std::format("cannot {} '{}': {}", what, path, cause.message())
        );
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

asset_cache::asset_cache(core::vfs::file_system &fileSystem, std::string_view cacheRoot, std::string_view platform) :
    mFileSystem(&fileSystem),
    mRoot(core::vfs::path_utils::normalize(core::vfs::path_utils::join(cacheRoot, platform))),
    mBlobRoot(core::vfs::path_utils::normalize(core::vfs::path_utils::join(mRoot, "blobs"))),
    mPlatform(platform) {}

asset_cache::~asset_cache() {}

/*===========================================================================*/
/*===========================================================================*/
// Public methods

std::string asset_cache::blob_path(const asset_id &id) const {
    const std::string text = id.to_string();
    std::string path = mBlobRoot;
    path.push_back('/');
    path.append(text, 0, 2);
    path.push_back('/');
    path.append(text);
    path.append(BLOB_EXTENSION);
    return path;
}

bool asset_cache::contains(const asset_id &id) const {
    return mFileSystem->exists(blob_path(id));
}

core::result<cooked_blob_header> asset_cache::read_header(const asset_id &id) const {
    const std::string path = blob_path(id);
    core::result<std::unique_ptr<core::vfs::file>> opened = mFileSystem->open_read(path);
    if (!opened.has_value()) {
        if (opened.get_error().generic_code() == core::error_code::NOT_FOUND) {
            return core::make_error(assets_error::CACHE_MISS, std::format("no blob for {}", id));
        }
        return io_error("open blob", path, opened.get_error());
    }

    std::vector<std::byte> bytes(cooked_blob_header::SIZE);
    core::result<usize> read = opened.value()->read_at(0, bytes);
    if (!read.has_value()) {
        return io_error("read blob", path, read.get_error());
    }
    bytes.resize(read.value());

    core::result<cooked_blob_header> header = parse_cooked_blob_header(bytes);
    if (!header.has_value()) {
        return header;
    }
    if (header.value().id != id) {
        return core::make_error(
          assets_error::CACHE_CORRUPT,
          std::format("blob of {} names {}", id, header.value().id)
        );
    }
    return header;
}

core::result<cooked_blob> asset_cache::read(const asset_id &id) const {
    const std::string path = blob_path(id);
    core::result<std::vector<std::byte>> bytes = mFileSystem->read_file(path);
    if (!bytes.has_value()) {
        if (bytes.get_error().generic_code() == core::error_code::NOT_FOUND) {
            return core::make_error(assets_error::CACHE_MISS, std::format("no blob for {}", id));
        }
        return io_error("read blob", path, bytes.get_error());
    }

    core::result<cooked_blob> blob = parse_cooked_blob(std::move(bytes).value());
    if (!blob.has_value()) {
        return blob;
    }
    if (blob.value().header.id != id) {
        return core::make_error(
          assets_error::CACHE_CORRUPT,
          std::format("blob of {} names {}", id, blob.value().header.id)
        );
    }
    return blob;
}

core::result<void> asset_cache::write(const cooked_blob &blob) const {
    const std::string path = blob_path(blob.header.id);
    const std::string temporary = path + std::string(TEMPORARY_SUFFIX);
    const std::vector<std::byte> bytes = write_cooked_blob(blob);

    core::result<void> written = mFileSystem->write_file(temporary, bytes);
    if (!written.has_value()) {
        return io_error("write blob", temporary, written.get_error());
    }
    core::result<void> renamed = mFileSystem->rename(temporary, path);
    if (!renamed.has_value()) {
        static_cast<void>(mFileSystem->remove(temporary));
        return io_error("rename blob", path, renamed.get_error());
    }
    return core::status::success();
}

bool asset_cache::is_valid(const cooked_blob_header &header, const cache_expectation &expected) noexcept {
    return header.formatVersion == COOKED_BLOB_FORMAT_VERSION && header.type == expected.type &&
      header.typeVersion == expected.typeVersion && header.importerVersion == expected.importerVersion &&
      header.sourceHash == expected.sourceHash && header.settingsHash == expected.settingsHash;
}

core::result<void> asset_cache::remove(const asset_id &id) const {
    const std::string path = blob_path(id);
    core::result<void> removed = mFileSystem->remove(path);
    if (!removed.has_value() && removed.get_error().generic_code() != core::error_code::NOT_FOUND) {
        return io_error("remove blob", path, removed.get_error());
    }
    return core::status::success();
}

core::result<u32> asset_cache::remove_temporaries() const {
    if (!mFileSystem->exists(mBlobRoot)) {
        return u32{0};
    }

    std::vector<std::string> temporaries = {};
    core::result<void> enumerated = mFileSystem->enumerate(
      mBlobRoot,
      core::vfs::enumerate_flags::RECURSIVE | core::vfs::enumerate_flags::FILES,
      [&temporaries](std::string_view path, const core::vfs::file_stat &) {
          if (core::str::ends_with(path, TEMPORARY_SUFFIX)) {
              temporaries.emplace_back(path);
          }
      }
    );
    if (!enumerated.has_value()) {
        return io_error("enumerate", mBlobRoot, enumerated.get_error());
    }

    u32 removed = 0;
    for (const std::string &path : temporaries) {
        core::result<void> deleted = mFileSystem->remove(path);
        if (deleted.has_value()) {
            ++removed;
        } else {
            CONJURE_LOG_WARN(LOG_ASSETS, "cannot remove temporary '{}': {}", path, deleted.get_error().message());
        }
    }
    return removed;
}

core::result<void> asset_cache::clear() const {
    if (!mFileSystem->exists(mBlobRoot)) {
        return core::status::success();
    }
    core::result<void> removed = mFileSystem->remove_all(mBlobRoot);
    if (!removed.has_value()) {
        return io_error("clear", mBlobRoot, removed.get_error());
    }
    return core::status::success();
}
