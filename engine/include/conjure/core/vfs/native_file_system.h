#pragma once

#include "conjure/core/api.h"
#include "conjure/core/vfs/file_system.h"

#include <filesystem>
#include <memory>
#include <string_view>

namespace conjure::core::vfs {

    /// <summary>
    /// A directory on disk exposed as a file_system. Every path is resolved under the root;
    /// anything that would escape it ("../x", "/../x", "C:/x", "//server/x") is PERMISSION_DENIED.
    /// Stateless apart from the root, so one instance may be used from any number of threads.
    /// Files are read through handles that share deletion, so an open reader never makes a
    /// write_file_atomic, rename or remove of the same file fail. write_file_atomic is durable:
    /// the new contents are flushed to the storage device (FlushFileBuffers, fsync) before they
    /// replace the old ones, at the cost of one device flush per call. Two limits: a volume whose
    /// driver has no flush at all still takes the write, without this guarantee, and logs one
    /// warning per process; on macOS the flush is fsync, which hands the data to the drive without
    /// waiting for the drive's own write cache, so the guarantee there is only as strong as that
    /// cache.
    /// </summary>
    class CONJURE_API native_file_system final : public file_system {
        private:
            std::filesystem::path mRoot = {};
            bool mReadOnly = false;

        public:
            /// <summary>
            /// Wraps 'rootPath'. The directory does not have to exist yet: the first write
            /// creates it. A read-only instance rejects every mutating call.
            /// </summary>
            explicit native_file_system(std::filesystem::path rootPath, bool readOnly = false);
            ~native_file_system() override;

        public:
            /// <summary>
            /// The root every path is resolved against, as given to the constructor (lexically
            /// normalised, without a trailing separator).
            /// </summary>
            [[nodiscard]] const std::filesystem::path &root() const noexcept {
                return mRoot;
            }

            /// <summary>
            /// The on-disk path a backend-relative path maps to; "" and "/" map to the root itself.
            /// Checked exactly like every file method: a path that would leave the root ("../x",
            /// "/../x", "C:/x", "//server/x") is PERMISSION_DENIED, so the result is always the
            /// root or below it and is safe to hand to code outside the file system (a process, a
            /// dynamic library). Purely lexical: the path does not have to exist, and a read-only
            /// instance maps paths like any other. On Windows a path beyond the classic length
            /// limit, or one with a name that is not portable (path_utils::is_portable_name), comes
            /// back in the extended-length form, the one that addresses it.
            /// </summary>
            [[nodiscard]] result<std::filesystem::path> to_native(std::string_view path) const;

            [[nodiscard]] result<std::vector<std::byte>> read_file(std::string_view path) override;
            [[nodiscard]] result<void> write_file(std::string_view path, std::span<const std::byte> data) override;
            [[nodiscard]] result<void> write_file_atomic(
              std::string_view path,
              std::span<const std::byte> data
            ) override;
            [[nodiscard]] result<file_stat> stat(std::string_view path) override;
            [[nodiscard]] result<void> enumerate(
              std::string_view directory,
              enumerate_flags flags,
              const enumerate_callback &callback
            ) override;
            [[nodiscard]] result<void> rename(std::string_view from, std::string_view to) override;
            [[nodiscard]] result<void> remove(std::string_view path) override;
            [[nodiscard]] result<void> remove_all(std::string_view path) override;
            [[nodiscard]] result<void> create_directories(std::string_view path) override;
            [[nodiscard]] result<std::unique_ptr<file>> open_read(std::string_view path) override;
            [[nodiscard]] bool is_read_only() const noexcept override;

        private:
            /// <summary>
            /// Maps a path that resolve_backend_path already accepted (normalised, relative, inside
            /// the root) to its on-disk path. Unchecked: only the checked entry points call it.
            /// </summary>
            [[nodiscard]] std::filesystem::path native_path_of(std::string_view relative) const;
            [[nodiscard]] result<std::filesystem::path> resolve(std::string_view path) const;
            [[nodiscard]] result<std::filesystem::path> resolve_writable(std::string_view path) const;
            [[nodiscard]] result<void> write_bytes(
              const std::filesystem::path &nativePath,
              std::string_view displayPath,
              std::span<const std::byte> data
            ) const;
    };
}
