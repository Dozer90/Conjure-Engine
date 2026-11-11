#pragma once

#include "conjure/core/api.h"
#include "conjure/core/vfs/file_system.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string_view>

namespace conjure::core::vfs {

    /// <summary>
    /// A file_system kept entirely in memory. Used by tests, by the cooker for staging output and
    /// wherever a scratch tree is needed. Every operation takes one mutex, so the instance is
    /// safe to share between threads; write times come from a counter that increases with every
    /// mutation, which keeps them deterministic.
    /// </summary>
    class CONJURE_API memory_file_system final : public file_system {
        private:
            struct node;

            std::unique_ptr<node> mRoot = nullptr;
            mutable std::mutex mMutex = {};
            u64 mWriteCounter = 0;                  // guarded by mMutex
            std::atomic<bool> mReadOnly = false;

        public:
            memory_file_system();
            ~memory_file_system() override;

        public:
            /// <summary>
            /// Freezes (or thaws) the tree: while read-only, every mutating call fails with
            /// PERMISSION_DENIED. Typically used after populating a fixture.
            /// </summary>
            void set_read_only(bool readOnly) noexcept;

            /// <summary>
            /// Number of mutations performed so far; equals the write time of the newest entry.
            /// </summary>
            [[nodiscard]] u64 write_counter() const;

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
            [[nodiscard]] result<void> check_writable() const;
            [[nodiscard]] result<node *> ensure_directory_locked(std::string_view normalizedPath);
            [[nodiscard]] result<void> remove_locked(std::string_view path, bool recursive);
    };
}
