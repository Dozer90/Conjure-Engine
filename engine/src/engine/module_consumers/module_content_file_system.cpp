#include "engine/module_consumers/module_content_file_system.h"

#include "conjure/core/vfs/path_utils.h"

#include <format>
#include <utility>

using namespace conjure;
using namespace conjure::engine;
using conjure::core::error;
using conjure::core::error_code;

namespace {

    /// <summary>
    /// Splits "alpha/textures/a.png" into "alpha" and "textures/a.png" (the remainder may be empty).
    /// </summary>
    [[nodiscard]] std::pair<std::string_view, std::string_view> split_first(std::string_view path) noexcept {
        usize start = 0;
        while (start < path.size() && (path[start] == '/' || path[start] == '\\')) {
            ++start;
        }
        const std::string_view trimmed = path.substr(start);
        const usize separator = trimmed.find_first_of("/\\");
        if (separator == std::string_view::npos) {
            return {trimmed, std::string_view()};
        }
        return {trimmed.substr(0, separator), trimmed.substr(separator + 1)};
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

module_content_file_system::module_content_file_system() {}

module_content_file_system::~module_content_file_system() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<void> module_content_file_system::add_root(
  std::string_view moduleName,
  std::shared_ptr<core::vfs::file_system> backend
) {
    if (moduleName.empty() || moduleName.find_first_of("/\\") != std::string_view::npos || backend == nullptr) {
        return error(error_code::INVALID_ARGUMENT, "module content root needs a plain name and a backend");
    }
    const std::lock_guard<std::mutex> lock(mMutex);
    if (mRoots.contains(moduleName)) {
        return error(error_code::ALREADY_EXISTS, std::format("module content '{}' is already mounted", moduleName));
    }
    mRoots.emplace(std::string(moduleName), std::move(backend));
    return core::result<void>::success();
}

core::result<void> module_content_file_system::remove_root(std::string_view moduleName) {
    const std::lock_guard<std::mutex> lock(mMutex);
    const auto found = mRoots.find(moduleName);
    if (found == mRoots.end()) {
        return error(error_code::NOT_FOUND, std::format("module content '{}' is not mounted", moduleName));
    }
    mRoots.erase(found);
    return core::result<void>::success();
}

std::vector<std::string> module_content_file_system::roots() const {
    const std::lock_guard<std::mutex> lock(mMutex);
    std::vector<std::string> names = {};
    names.reserve(mRoots.size());
    for (const auto &entry : mRoots) {
        names.push_back(entry.first);
    }
    return names;
}

core::result<std::vector<std::byte>> module_content_file_system::read_file(std::string_view path) {
    CONJURE_TRY_ASSIGN(const route target, resolve(path));
    return target.backend->read_file(target.remainder);
}

core::result<void> module_content_file_system::write_file(std::string_view path, std::span<const std::byte> data) {
    CONJURE_TRY_ASSIGN(const route target, resolve(path));
    return target.backend->write_file(target.remainder, data);
}

core::result<void> module_content_file_system::write_file_atomic(
  std::string_view path,
  std::span<const std::byte> data
) {
    CONJURE_TRY_ASSIGN(const route target, resolve(path));
    return target.backend->write_file_atomic(target.remainder, data);
}

core::result<core::vfs::file_stat> module_content_file_system::stat(std::string_view path) {
    const std::pair<std::string_view, std::string_view> parts = split_first(path);
    if (parts.first.empty()) {
        core::vfs::file_stat root = {};
        root.isDirectory = true;
        return root;
    }
    CONJURE_TRY_ASSIGN(const route target, resolve(path));
    return target.backend->stat(target.remainder);
}

core::result<void> module_content_file_system::enumerate(
  std::string_view directory,
  core::vfs::enumerate_flags flags,
  const core::vfs::enumerate_callback &callback
) {
    const std::pair<std::string_view, std::string_view> parts = split_first(directory);
    if (parts.first.empty()) {
        // The root lists every module as a directory (and their trees when recursive).
        std::vector<std::pair<std::string, std::shared_ptr<core::vfs::file_system>>> roots = {};
        {
            const std::lock_guard<std::mutex> lock(mMutex);
            roots.assign(mRoots.begin(), mRoots.end());
        }
        const bool wantDirectories = !has_flag(flags, core::vfs::enumerate_flags::FILES)
          || has_flag(flags, core::vfs::enumerate_flags::DIRECTORIES);
        for (const auto &entry : roots) {
            if (wantDirectories) {
                core::vfs::file_stat stat = {};
                stat.isDirectory = true;
                callback(entry.first, stat);
            }
            if (!has_flag(flags, core::vfs::enumerate_flags::RECURSIVE)) {
                continue;
            }
            const std::string prefix = entry.first;
            const core::vfs::enumerate_callback forward = [&](std::string_view path, const core::vfs::file_stat &stat) {
                callback(prefix + "/" + std::string(path), stat);
            };
            CONJURE_TRY(entry.second->enumerate("", flags, forward));
        }
        return core::result<void>::success();
    }

    CONJURE_TRY_ASSIGN(const route target, resolve(directory));
    const std::string prefix = std::string(parts.first);
    const core::vfs::enumerate_callback forward = [&](std::string_view path, const core::vfs::file_stat &stat) {
        callback(prefix + "/" + std::string(path), stat);
    };
    return target.backend->enumerate(target.remainder, flags, forward);
}

core::result<void> module_content_file_system::rename(std::string_view from, std::string_view to) {
    CONJURE_TRY_ASSIGN(const route source, resolve(from));
    CONJURE_TRY_ASSIGN(const route destination, resolve(to));
    if (source.backend != destination.backend) {
        return error(error_code::UNSUPPORTED, "renaming across module content roots is not supported");
    }
    return source.backend->rename(source.remainder, destination.remainder);
}

core::result<void> module_content_file_system::remove(std::string_view path) {
    CONJURE_TRY_ASSIGN(const route target, resolve(path));
    return target.backend->remove(target.remainder);
}

core::result<void> module_content_file_system::create_directories(std::string_view path) {
    CONJURE_TRY_ASSIGN(const route target, resolve(path));
    return target.backend->create_directories(target.remainder);
}

core::result<std::unique_ptr<core::vfs::file>> module_content_file_system::open_read(std::string_view path) {
    CONJURE_TRY_ASSIGN(const route target, resolve(path));
    return target.backend->open_read(target.remainder);
}

bool module_content_file_system::is_read_only() const noexcept {
    const std::lock_guard<std::mutex> lock(mMutex);
    if (mRoots.empty()) {
        return false;
    }
    for (const auto &entry : mRoots) {
        if (!entry.second->is_read_only()) {
            return false;
        }
    }
    return true;
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

core::result<module_content_file_system::route> module_content_file_system::resolve(std::string_view path) const {
    const std::pair<std::string_view, std::string_view> parts = split_first(path);
    if (parts.first.empty()) {
        return error(error_code::INVALID_ARGUMENT, "a module content path starts with the module name");
    }
    const std::lock_guard<std::mutex> lock(mMutex);
    const auto found = mRoots.find(parts.first);
    if (found == mRoots.end()) {
        return error(error_code::NOT_FOUND, std::format("no module content mounted as '{}'", parts.first));
    }
    route target = {};
    target.backend = found->second;
    target.remainder = std::string(parts.second);
    return target;
}
