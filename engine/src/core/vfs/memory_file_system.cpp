#include "conjure/core/vfs/memory_file_system.h"

#include "conjure/core/vfs/path_utils.h"
#include "core/vfs/vfs_internal.h"

#include <algorithm>
#include <format>
#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::vfs;

/*===========================================================================*/
/*===========================================================================*/
// Tree node

struct memory_file_system::node {
    using byte_buffer = std::vector<std::byte>;

    bool isDirectory = false;
    u64 writeTime = 0;
    // File contents are immutable once stored: a write swaps the pointer, so files opened
    // earlier keep reading the snapshot they were opened on without copying.
    std::shared_ptr<const byte_buffer> data = nullptr;
    std::map<std::string, std::unique_ptr<node>, std::less<>> children = {};

    [[nodiscard]] static std::unique_ptr<node> make_directory(u64 time) {
        std::unique_ptr<node> created = std::make_unique<node>();
        created->isDirectory = true;
        created->writeTime = time;
        return created;
    }

    [[nodiscard]] static std::unique_ptr<node> make_file(std::span<const std::byte> bytes, u64 time) {
        std::unique_ptr<node> created = std::make_unique<node>();
        created->isDirectory = false;
        created->writeTime = time;
        created->data = std::make_shared<byte_buffer>(bytes.begin(), bytes.end());
        return created;
    }

    [[nodiscard]] node *find(std::string_view normalizedPath) noexcept {
        node *current = this;
        for (const std::string_view segment : internal::split_segments(normalizedPath)) {
            if (!current->isDirectory) {
                return nullptr;
            }
            const auto found = current->children.find(segment);
            if (found == current->children.end()) {
                return nullptr;
            }
            current = found->second.get();
        }
        return current;
    }

    [[nodiscard]] file_stat to_stat() const noexcept {
        file_stat info = {};
        info.isDirectory = isDirectory;
        info.isRegular = !isDirectory;
        info.writeTimeNanoseconds = writeTime;
        if (!isDirectory && data != nullptr) {
            info.size = data->size();
        }
        return info;
    }

    void collect(
      const std::string &prefix,
      enumerate_flags flags,
      std::vector<std::pair<std::string, file_stat>> &entries
    ) const {
        for (const auto &[name, child] : children) {
            const std::string childPath = path_utils::join(prefix, name);
            if (internal::passes_kind_filter(flags, child->isDirectory)) {
                entries.emplace_back(childPath, child->to_stat());
            }
            if (child->isDirectory && has_flag(flags, enumerate_flags::RECURSIVE)) {
                child->collect(childPath, flags, entries);
            }
        }
    }
};


/*===========================================================================*/
/*===========================================================================*/
// Helpers

namespace {
    using byte_buffer = std::vector<std::byte>;

    error not_found(std::string_view path) {
        return make_error(error_code::NOT_FOUND, std::format("'{}' does not exist", path));
    }

    error is_a_directory(std::string_view path) {
        return make_error(error_code::INVALID_ARGUMENT, std::format("'{}' is a directory", path));
    }

    error root_is_immutable() {
        return make_error(error_code::INVALID_ARGUMENT, "the root of a file system cannot be removed or renamed");
    }

    /// <summary>
    /// Reads from an immutable snapshot of a file's contents, so no locking is needed.
    /// </summary>
    class memory_file final : public file {
        private:
            std::shared_ptr<const byte_buffer> mData = nullptr;
            std::string mDisplayPath = {};

        public:
            memory_file(std::shared_ptr<const byte_buffer> data, std::string displayPath) :
                mData(std::move(data)),
                mDisplayPath(std::move(displayPath)) {}

            ~memory_file() override {}

        public:
            [[nodiscard]] result<usize> read_at(u64 offset, std::span<std::byte> out) override {
                const u64 total = size();
                if (offset > total) {
                    return make_error(
                      error_code::OUT_OF_RANGE,
                      std::format("offset {} is past the end of '{}' ({} bytes)", offset, mDisplayPath, total)
                    );
                }
                const usize toRead = static_cast<usize>(std::min<u64>(total - offset, out.size()));
                if (toRead > 0) {
                    std::copy_n(mData->begin() + static_cast<std::ptrdiff_t>(offset), toRead, out.begin());
                }
                return toRead;
            }

            [[nodiscard]] u64 size() const noexcept override {
                return mData == nullptr ? 0 : mData->size();
            }
    };
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

memory_file_system::memory_file_system() :
    mRoot(node::make_directory(0)) {}

memory_file_system::~memory_file_system() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

void memory_file_system::set_read_only(bool readOnly) noexcept {
    mReadOnly.store(readOnly, std::memory_order_release);
}

u64 memory_file_system::write_counter() const {
    const std::scoped_lock lock(mMutex);
    return mWriteCounter;
}

result<std::vector<std::byte>> memory_file_system::read_file(std::string_view path) {
    CONJURE_TRY_ASSIGN(const std::string normalized, internal::resolve_backend_path(path));

    const std::scoped_lock lock(mMutex);
    const node *found = mRoot->find(normalized);
    if (found == nullptr) {
        return not_found(path);
    }
    if (found->isDirectory) {
        return is_a_directory(path);
    }
    return std::vector<std::byte>(*found->data);
}

result<void> memory_file_system::write_file(std::string_view path, std::span<const std::byte> data) {
    CONJURE_TRY_ASSIGN(const std::string normalized, internal::resolve_backend_path(path));
    CONJURE_TRY(check_writable());
    if (normalized.empty()) {
        return is_a_directory(path);
    }
    CONJURE_TRY(internal::validate_new_path(normalized, path));

    const std::scoped_lock lock(mMutex);
    CONJURE_TRY_ASSIGN(node *parent, ensure_directory_locked(path_utils::parent(normalized)));

    const std::string_view name = path_utils::filename(normalized);
    const auto existing = parent->children.find(name);
    if (existing != parent->children.end() && existing->second->isDirectory) {
        return is_a_directory(path);
    }

    const u64 time = ++mWriteCounter;
    if (existing == parent->children.end()) {
        parent->children.emplace(std::string(name), node::make_file(data, time));
        parent->writeTime = time;
        return result<void>::success();
    }
    existing->second->data = std::make_shared<byte_buffer>(data.begin(), data.end());
    existing->second->writeTime = time;
    return result<void>::success();
}

result<void> memory_file_system::write_file_atomic(std::string_view path, std::span<const std::byte> data) {
    // Every write already swaps the whole contents under the lock, which is as atomic as it gets, and
    // there is no device to flush: the tree lives and dies with the process.
    return write_file(path, data);
}

result<file_stat> memory_file_system::stat(std::string_view path) {
    CONJURE_TRY_ASSIGN(const std::string normalized, internal::resolve_backend_path(path));

    const std::scoped_lock lock(mMutex);
    const node *found = mRoot->find(normalized);
    if (found == nullptr) {
        return not_found(path);
    }
    return found->to_stat();
}

result<void> memory_file_system::enumerate(
  std::string_view directory,
  enumerate_flags flags,
  const enumerate_callback &callback
) {
    CONJURE_TRY_ASSIGN(const std::string normalized, internal::resolve_backend_path(directory));

    // Entries are gathered under the lock and reported after it is released, so a callback may
    // call back into this file system.
    std::vector<std::pair<std::string, file_stat>> entries = {};
    {
        const std::scoped_lock lock(mMutex);
        const node *found = mRoot->find(normalized);
        if (found == nullptr) {
            return not_found(directory);
        }
        if (!found->isDirectory) {
            return make_error(error_code::INVALID_ARGUMENT, std::format("'{}' is not a directory", directory));
        }
        found->collect(normalized, flags, entries);
    }

    std::sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
        return left.first < right.first;
    });
    for (const auto &[entryPath, entryStat] : entries) {
        callback(entryPath, entryStat);
    }
    return result<void>::success();
}

result<void> memory_file_system::rename(std::string_view from, std::string_view to) {
    CONJURE_TRY_ASSIGN(const std::string source, internal::resolve_backend_path(from));
    CONJURE_TRY_ASSIGN(const std::string destination, internal::resolve_backend_path(to));
    CONJURE_TRY(check_writable());
    if (source.empty() || destination.empty()) {
        return root_is_immutable();
    }
    // Only the destination is a name this call creates; the source may be any name that exists.
    CONJURE_TRY(internal::validate_new_path(destination, to));

    const std::scoped_lock lock(mMutex);
    node *sourceParent = mRoot->find(path_utils::parent(source));
    if (sourceParent == nullptr || !sourceParent->isDirectory) {
        return not_found(from);
    }
    const auto sourceEntry = sourceParent->children.find(path_utils::filename(source));
    if (sourceEntry == sourceParent->children.end()) {
        return not_found(from);
    }
    if (source == destination) {
        return result<void>::success();
    }

    node *moving = sourceEntry->second.get();
    if (moving->isDirectory && internal::is_same_or_below(destination, source)) {
        return make_error(error_code::INVALID_ARGUMENT, std::format("cannot move '{}' into itself", from));
    }

    node *destinationParent = mRoot->find(path_utils::parent(destination));
    if (destinationParent == nullptr || !destinationParent->isDirectory) {
        return make_error(error_code::NOT_FOUND, std::format("the parent of '{}' does not exist", to));
    }
    const std::string_view destinationName = path_utils::filename(destination);
    const auto destinationEntry = destinationParent->children.find(destinationName);
    if (destinationEntry != destinationParent->children.end()) {
        if (destinationEntry->second->isDirectory) {
            return make_error(error_code::ALREADY_EXISTS, std::format("a directory exists at '{}'", to));
        }
        if (moving->isDirectory) {
            return make_error(error_code::ALREADY_EXISTS, std::format("a file exists at '{}'", to));
        }
    }

    const u64 time = ++mWriteCounter;
    std::unique_ptr<node> detached = std::move(sourceEntry->second);
    sourceParent->children.erase(sourceEntry);
    sourceParent->writeTime = time;
    destinationParent->children.insert_or_assign(std::string(destinationName), std::move(detached));
    destinationParent->writeTime = time;
    return result<void>::success();
}

result<void> memory_file_system::remove(std::string_view path) {
    return remove_locked(path, false);
}

result<void> memory_file_system::remove_all(std::string_view path) {
    return remove_locked(path, true);
}

result<void> memory_file_system::create_directories(std::string_view path) {
    CONJURE_TRY_ASSIGN(const std::string normalized, internal::resolve_backend_path(path));
    CONJURE_TRY(check_writable());
    CONJURE_TRY(internal::validate_new_path(normalized, path));

    const std::scoped_lock lock(mMutex);
    const node *existing = mRoot->find(normalized);
    if (existing != nullptr && !existing->isDirectory) {
        return make_error(error_code::ALREADY_EXISTS, std::format("a file exists at '{}'", path));
    }
    CONJURE_TRY(ensure_directory_locked(normalized));
    return result<void>::success();
}

result<std::unique_ptr<file>> memory_file_system::open_read(std::string_view path) {
    CONJURE_TRY_ASSIGN(const std::string normalized, internal::resolve_backend_path(path));

    const std::scoped_lock lock(mMutex);
    const node *found = mRoot->find(normalized);
    if (found == nullptr) {
        return not_found(path);
    }
    if (found->isDirectory) {
        return is_a_directory(path);
    }
    std::unique_ptr<file> opened = std::make_unique<memory_file>(found->data, std::string(path));
    return opened;
}

bool memory_file_system::is_read_only() const noexcept {
    return mReadOnly.load(std::memory_order_acquire);
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

result<void> memory_file_system::check_writable() const {
    if (is_read_only()) {
        return make_error(error_code::PERMISSION_DENIED, "the file system is read-only");
    }
    return result<void>::success();
}

result<memory_file_system::node *> memory_file_system::ensure_directory_locked(std::string_view normalizedPath) {
    node *current = mRoot.get();
    std::string walked = {};
    for (const std::string_view segment : internal::split_segments(normalizedPath)) {
        walked = path_utils::join(walked, segment);
        const auto found = current->children.find(segment);
        if (found != current->children.end()) {
            if (!found->second->isDirectory) {
                return make_error(error_code::IO_FAILURE, std::format("'{}' is a file, not a directory", walked));
            }
            current = found->second.get();
            continue;
        }

        const u64 time = ++mWriteCounter;
        std::unique_ptr<node> created = node::make_directory(time);
        node *createdPointer = created.get();
        current->children.emplace(std::string(segment), std::move(created));
        current->writeTime = time;
        current = createdPointer;
    }
    return current;
}

result<void> memory_file_system::remove_locked(std::string_view path, bool recursive) {
    CONJURE_TRY_ASSIGN(const std::string normalized, internal::resolve_backend_path(path));
    CONJURE_TRY(check_writable());
    if (normalized.empty()) {
        return root_is_immutable();
    }

    const std::scoped_lock lock(mMutex);
    node *parent = mRoot->find(path_utils::parent(normalized));
    if (parent == nullptr || !parent->isDirectory) {
        return not_found(path);
    }
    const auto entry = parent->children.find(path_utils::filename(normalized));
    if (entry == parent->children.end()) {
        return not_found(path);
    }
    if (!recursive && entry->second->isDirectory && !entry->second->children.empty()) {
        return make_error(error_code::INVALID_STATE, std::format("directory '{}' is not empty", path));
    }

    parent->children.erase(entry);
    parent->writeTime = ++mWriteCounter;
    return result<void>::success();
}
