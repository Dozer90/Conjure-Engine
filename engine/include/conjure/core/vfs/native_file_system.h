#pragma once

#include "conjure/core/api.h"
#include "conjure/core/vfs/file_system.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
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

    /// <summary>
    /// Writes 'data' to the file at the on-disk path 'path' so that only the user the process runs as may
    /// open it: the place for a secret that other accounts of the machine must not read. The data goes
    /// into a new file beside 'path' that is created with the restriction already in place (POSIX: mode
    /// 0600; Windows: an access control list that names that user alone, inherits nothing from the folder
    /// and makes the user the owner), and that file then takes the place of 'path' in one step. A reader
    /// therefore sees the old contents or the new ones, never a part, and what was at 'path' before (a
    /// file with wider permissions, a symbolic link) is replaced, not written through. The folder must
    /// exist. 'path' must name a file: an empty path, one that ends in a separator and one that is a
    /// directory are INVALID_ARGUMENT, and nothing is created.
    ///
    /// Limits. UNSUPPORTED, with nothing written, when the volume cannot restrict a file to its owner:
    /// FAT and exFAT, and POSIX mounts whose owner and mode are fixed by mount options. Accounts the
    /// operating system lets pass any permission still read the file (root; on Windows administrators
    /// and backup operators). Access control lists beyond the mode bits are not examined on POSIX
    /// (macOS). The folder is the caller's business: an account that may write it can replace or delete
    /// the file, though not read it. The data is not flushed to the device, so a power cut may lose it.
    /// Every other failure is IO_FAILURE with the operating system's text. The data is written only once
    /// the restriction was checked on the new file, and a failure removes that file again; should the
    /// removal fail as well, it stays beside 'path' as "&lt;name&gt;.tmp-&lt;uuid&gt;", either holding
    /// none of the data or restricted to its owner.
    /// </summary>
    [[nodiscard]] CONJURE_API result<void> write_private_file(
      const std::filesystem::path &path,
      std::span<const std::byte> data
    );

    /// <summary>
    /// Whether the entry at the on-disk path 'path' is a regular file that only the user the process runs
    /// as may open, by the rule write_private_file establishes. POSIX: a regular file (a symbolic link is
    /// not followed and is never private) owned by the effective user with no permission bit for its
    /// group or for others. Windows: a file that is not a reparse point, on a volume that keeps access
    /// control lists, owned by the process user, whose list is protected from inheritance and holds
    /// nothing but entries allowing that user. The limits of write_private_file apply (privileged
    /// accounts, access control lists on POSIX). NOT_FOUND when nothing is at 'path', IO_FAILURE when the
    /// entry cannot be inspected. A file another user made private is IO_FAILURE on Windows, where it
    /// cannot be opened to look, and false on POSIX.
    /// </summary>
    [[nodiscard]] CONJURE_API result<bool> is_private_file(const std::filesystem::path &path);
}
