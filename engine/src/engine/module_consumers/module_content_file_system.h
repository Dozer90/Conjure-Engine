#pragma once

#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/core/vfs/file_system.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

// The backend behind modules:// (docs/design/09_cross_area_resolutions.md R24): the first path segment
// names a loaded module ("modules://alpha/textures/a.png" -> the backend mounted for "alpha" at
// "textures/a.png"). One instance is mounted under the "modules" scheme; the assets module consumer adds
// and removes module roots as modules load and unload. Guarded by a mutex like core's mount_table.

namespace conjure::engine {

    class module_content_file_system final : public core::vfs::file_system {
        private:
            struct route {
                std::shared_ptr<core::vfs::file_system> backend = nullptr;
                std::string remainder = {};
            };

            mutable std::mutex mMutex = {};
            std::map<std::string, std::shared_ptr<core::vfs::file_system>, std::less<>> mRoots = {};

        public:
            module_content_file_system();
            ~module_content_file_system() override;

        public:
            /// <summary>
            /// Binds a module name to a backend. ALREADY_EXISTS for a repeated name, INVALID_ARGUMENT for an
            /// empty name, a name containing '/' or a null backend.
            /// </summary>
            [[nodiscard]] core::result<void> add_root(
              std::string_view moduleName,
              std::shared_ptr<core::vfs::file_system> backend
            );

            /// <summary>
            /// Removes a binding. NOT_FOUND when unknown.
            /// </summary>
            [[nodiscard]] core::result<void> remove_root(std::string_view moduleName);

            /// <summary>
            /// Every bound module name in ascending order.
            /// </summary>
            [[nodiscard]] std::vector<std::string> roots() const;

            [[nodiscard]] core::result<std::vector<std::byte>> read_file(std::string_view path) override;
            [[nodiscard]] core::result<void> write_file(
              std::string_view path,
              std::span<const std::byte> data
            ) override;
            [[nodiscard]] core::result<void> write_file_atomic(
              std::string_view path,
              std::span<const std::byte> data
            ) override;
            [[nodiscard]] core::result<core::vfs::file_stat> stat(std::string_view path) override;
            [[nodiscard]] core::result<void> enumerate(
              std::string_view directory,
              core::vfs::enumerate_flags flags,
              const core::vfs::enumerate_callback &callback
            ) override;
            [[nodiscard]] core::result<void> rename(std::string_view from, std::string_view to) override;
            [[nodiscard]] core::result<void> remove(std::string_view path) override;
            [[nodiscard]] core::result<void> create_directories(std::string_view path) override;
            [[nodiscard]] core::result<std::unique_ptr<core::vfs::file>> open_read(std::string_view path) override;
            [[nodiscard]] bool is_read_only() const noexcept override;

        private:
            [[nodiscard]] core::result<route> resolve(std::string_view path) const;
    };
}
