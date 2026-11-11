#pragma once

#include "conjure/core/api.h"
#include "conjure/core/vfs/file_system.h"

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace conjure::core::vfs {

    /// <summary>
    /// Routes "scheme://path" URIs to mounted backends. The engine mounts project://, assets://,
    /// engine://, cooked:// and modules:// (module content lives under modules://<name>/).
    /// Every file_system method accepts URIs only; a bare path is INVALID_ARGUMENT and an
    /// unmounted scheme is NOT_FOUND. Paths handed to enumerate callbacks carry their scheme so
    /// they can be fed straight back in. Mount, unmount and lookup are guarded by a mutex;
    /// backends are held by shared_ptr so an operation in flight keeps its backend alive even
    /// when the scheme is unmounted concurrently.
    /// </summary>
    class CONJURE_API mount_table final : public file_system {
        private:
            struct route_target {
                std::shared_ptr<file_system> backend = nullptr;
                std::string_view scheme = {};
                std::string_view path = {};
            };

            mutable std::mutex mMutex = {};
            std::map<std::string, std::shared_ptr<file_system>, std::less<>> mMounts = {};

        public:
            mount_table();
            ~mount_table() override;

        public:
            /// <summary>
            /// Binds a scheme ("assets" or "assets://") to a backend. INVALID_ARGUMENT for a null
            /// backend, a malformed scheme or a mount table that already contains this one (directly
            /// or through other mount tables), ALREADY_EXISTS when the scheme is taken. One backend
            /// may be mounted under several schemes.
            /// </summary>
            [[nodiscard]] result<void> mount(std::string_view scheme, std::shared_ptr<file_system> backend);

            /// <summary>
            /// Removes a binding. NOT_FOUND when the scheme is not mounted.
            /// </summary>
            [[nodiscard]] result<void> unmount(std::string_view scheme);

            /// <summary>
            /// The backend bound to a scheme ("assets" or "assets://"), or nullptr. The pointer
            /// stays valid until the scheme is unmounted.
            /// </summary>
            [[nodiscard]] file_system *find_mount(std::string_view scheme) const noexcept;

            /// <summary>
            /// Every mounted scheme in ascending order.
            /// </summary>
            [[nodiscard]] std::vector<std::string> schemes() const;

            /// <summary>
            /// Splits "scheme://path" into its two parts ("assets://a/b.png" -> {"assets",
            /// "a/b.png"}). INVALID_ARGUMENT when there is no "://" or the scheme is empty or
            /// contains characters other than letters, digits, '-', '_', '.' and '+'.
            /// </summary>
            [[nodiscard]] static result<std::pair<std::string_view, std::string_view>> split(std::string_view uri);

            [[nodiscard]] result<std::vector<std::byte>> read_file(std::string_view uri) override;
            [[nodiscard]] result<std::string> read_text_file(std::string_view uri) override;
            [[nodiscard]] result<void> write_file(std::string_view uri, std::span<const std::byte> data) override;
            [[nodiscard]] result<void> write_file_atomic(
              std::string_view uri,
              std::span<const std::byte> data
            ) override;
            [[nodiscard]] result<file_stat> stat(std::string_view uri) override;
            [[nodiscard]] bool exists(std::string_view uri) override;
            [[nodiscard]] result<void> enumerate(
              std::string_view directory,
              enumerate_flags flags,
              const enumerate_callback &callback
            ) override;

            /// <summary>
            /// When both URIs resolve to the same backend it renames (so two schemes over one
            /// backend behave like one). Across backends a file is copied and the source removed; a
            /// directory is UNSUPPORTED.
            /// </summary>
            [[nodiscard]] result<void> rename(std::string_view from, std::string_view to) override;
            [[nodiscard]] result<void> remove(std::string_view uri) override;
            [[nodiscard]] result<void> remove_all(std::string_view uri) override;
            [[nodiscard]] result<void> create_directories(std::string_view uri) override;
            [[nodiscard]] result<std::unique_ptr<file>> open_read(std::string_view uri) override;

            /// <summary>
            /// True when at least one backend is mounted and all of them are read-only.
            /// </summary>
            [[nodiscard]] bool is_read_only() const noexcept override;

        private:
            [[nodiscard]] result<route_target> route(std::string_view uri) const;
            [[nodiscard]] std::shared_ptr<file_system> find_shared(std::string_view scheme) const;
            [[nodiscard]] std::vector<std::shared_ptr<file_system>> snapshot_backends() const;
            [[nodiscard]] bool contains(const file_system &backend) const;
    };
}
