#pragma once

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>
#include <system_error>

// The pieces of the native backend that need operating system calls: replacing a file in a single
// step and reading a file through a handle that lets other callers rename or delete it meanwhile
// (std::ifstream denies that on Windows). The free functions are implemented once per platform in
// native_platform_<platform>.cpp; read_handle itself lives in native_platform.cpp.

namespace conjure::core::vfs::internal {

    /// <summary>
    /// Owns an operating system file handle opened for reading (a HANDLE on Windows, a descriptor
    /// on POSIX) and closes it on destruction. Move-only.
    /// </summary>
    class read_handle final {
        private:
            static constexpr std::intptr_t INVALID_VALUE = -1;

            std::intptr_t mValue = INVALID_VALUE;

        public:
            read_handle() = default;
            explicit read_handle(std::intptr_t value) noexcept;
            ~read_handle();

            read_handle(const read_handle &) = delete;
            read_handle &operator=(const read_handle &) = delete;
            read_handle(read_handle &&other) noexcept;
            read_handle &operator=(read_handle &&other) noexcept;

        public:
            /// <summary>
            /// True while a handle is held.
            /// </summary>
            [[nodiscard]] bool is_open() const noexcept {
                return mValue != INVALID_VALUE;
            }

            /// <summary>
            /// The raw platform handle, meaningful only to the platform functions below.
            /// </summary>
            [[nodiscard]] std::intptr_t value() const noexcept {
                return mValue;
            }

            /// <summary>
            /// Closes the handle now; a second call does nothing.
            /// </summary>
            void close() noexcept;
    };

    /// <summary>
    /// Opens a regular file for shared reading: other callers may still write, rename and delete
    /// it. IO_FAILURE carries the operating system's text; 'displayPath' is used in messages.
    /// </summary>
    [[nodiscard]] result<read_handle> open_read_handle(const std::filesystem::path &path, std::string_view displayPath);

    /// <summary>
    /// The current size of the file behind an open handle.
    /// </summary>
    [[nodiscard]] result<u64> read_handle_size(const read_handle &handle, std::string_view displayPath);

    /// <summary>
    /// Positional read: fills 'out' from 'offset' and returns how many bytes were read, which is
    /// fewer than requested only at the end of the file. Safe to call from several threads on the
    /// same handle because no file position is shared.
    /// </summary>
    [[nodiscard]] result<usize> read_at(
      const read_handle &handle,
      u64 offset,
      std::span<std::byte> out,
      std::string_view displayPath
    );

    /// <summary>
    /// True when a lookup of 'path' that failed with 'code' did so because the entry is in the
    /// middle of being deleted: Windows answers "access denied" for a file whose last name is being
    /// removed while a handle to it is still open, and such a path counts as missing rather than as
    /// refused. Always false on POSIX, where unlinking is immediate.
    /// </summary>
    [[nodiscard]] bool is_being_deleted(const std::filesystem::path &path, const std::error_code &code) noexcept;

    /// <summary>
    /// Releases a raw handle obtained through open_read_handle. Used by read_handle only.
    /// </summary>
    void close_read_handle(std::intptr_t value) noexcept;

    /// <summary>
    /// Moves 'source' over 'target' so that readers see either the old or the new file, never a
    /// mix. 'target' may or may not exist; an existing file there is replaced even while another
    /// caller holds it open through open_read_handle.
    /// </summary>
    [[nodiscard]] result<void> replace_file(const std::filesystem::path &source, const std::filesystem::path &target);
}
