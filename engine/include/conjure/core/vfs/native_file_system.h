#pragma once

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
    /// write_file_atomic, rename or remove of the same file fail.
    /// </summary>
    class native_file_system final : public file_system {
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
            /// The on-disk path a backend-relative path maps to, without checking that it stays
            /// inside the root; "" maps to the root itself.
            /// </summary>
            [[nodiscard]] std::filesystem::path to_native(std::string_view path) const;

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
            [[nodiscard]] result<std::filesystem::path> resolve(std::string_view path) const;
            [[nodiscard]] result<std::filesystem::path> resolve_writable(std::string_view path) const;
            [[nodiscard]] result<void> write_bytes(
              const std::filesystem::path &nativePath,
              std::span<const std::byte> data
            ) const;
    };
}
