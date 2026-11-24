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
// step, writing a file so that it has reached the storage device, creating a file only its owner may
// open, and reading a file through a handle
// that lets other callers rename or delete it meanwhile (std::ifstream denies that on Windows). The
// free functions are implemented once per platform in native_platform_<platform>.cpp; read_handle and
// the flush statistics live in native_platform.cpp.

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
    /// What a lookup the operating system refused means; see classify_refused_lookup.
    /// </summary>
    enum class refused_lookup : u8 {
        /// A real refusal (permissions, an I/O error), reported as one.
        REFUSED,
        /// The entry is in the middle of being deleted: Windows answers "access denied" for a file
        /// whose last name is being removed while a handle to it is still open, and such a path
        /// counts as missing rather than as refused.
        DELETING,
        /// Windows answered "access denied", but the name answers a probe now. Either
        /// the file it named was being deleted and another file has taken the name since (an
        /// atomic write or a rename landing on it), or the refusal is real: the caller looks the
        /// path up again, and a lookup refused on every one of REFUSED_LOOKUP_ATTEMPTS is reported
        /// as refused.
        TRY_AGAIN,
    };

    /// How many times in all a lookup answered with refused_lookup::TRY_AGAIN is made.
    inline constexpr int REFUSED_LOOKUP_ATTEMPTS = 4;

    /// <summary>
    /// Classifies a lookup of 'path' that failed with 'code'. Always REFUSED on POSIX, where
    /// unlinking is immediate and "permission denied" is never transient.
    /// </summary>
    [[nodiscard]] refused_lookup classify_refused_lookup(
      const std::filesystem::path &path,
      const std::error_code &code
    ) noexcept;

    /// <summary>
    /// Releases a raw handle obtained through open_read_handle. Used by read_handle only.
    /// </summary>
    void close_read_handle(std::intptr_t value) noexcept;

    /// <summary>
    /// Creates or truncates the regular file at 'path', writes 'data' and has the operating system
    /// push the file to its storage device before returning: FlushFileBuffers on Windows, fsync on
    /// POSIX. After a successful return the contents are on the device: a crash or a power cut cannot
    /// leave the name pointing at torn or missing data, so the file may replace another one
    /// (write_file_atomic, code sweep F243). The directory entry is not flushed, so a file created by
    /// this call may itself be gone after a power cut. Two limits: a volume whose driver has no flush
    /// at all (flush_is_unsupported) still takes the write, without this guarantee, and logs one
    /// warning per process; on macOS the flush is fsync, which hands the data to the drive without
    /// waiting for the drive's own write cache, so the guarantee there is only as strong as that
    /// cache. The parent directory must exist. On failure the file may be left behind partly written;
    /// the caller removes it. IO_FAILURE carries the operating system's text; 'displayPath' is used in
    /// messages.
    /// </summary>
    [[nodiscard]] result<void> write_file_flushed(
      const std::filesystem::path &path,
      std::span<const std::byte> data,
      std::string_view displayPath
    );

    /// <summary>
    /// Creates the regular file at 'path', which must not exist yet, so that only the user the process
    /// runs as may open it, and writes 'data'. The restriction is part of the creation, so the file is
    /// never visible with wider access: on POSIX it is created with mode 0600 (open with O_EXCL, then
    /// fchmod, so neither a link planted at the path nor the process umask changes the outcome), on
    /// Windows with an access control list that names that user alone, is protected from the folder's
    /// inheritable entries and makes the user the owner. The result is then checked on the open file
    /// (is_private_file's rule): a volume that did not apply the restriction, such as FAT or exFAT, or
    /// a POSIX mount whose owner and mode come from its mount options, answers UNSUPPORTED and the file
    /// is removed again, before any of 'data' was written. The parent directory must exist. The data is
    /// not flushed to the device. Every other failure is IO_FAILURE with the operating system's text, and
    /// leaves no file behind unless removing it failed as well; 'displayPath' is used in messages.
    /// </summary>
    [[nodiscard]] result<void> create_private_file(
      const std::filesystem::path &path,
      std::span<const std::byte> data,
      std::string_view displayPath
    );

    /// <summary>
    /// Whether the entry at 'path' is a regular file that only the user the process runs as may open.
    /// POSIX: the entry itself (a symbolic link is not followed and is never private) is a regular file
    /// owned by the effective user with no permission bit for its group or for others; access control
    /// lists beyond the mode bits (macOS) are not read. Windows: the entry is a file and not a reparse
    /// point, its volume keeps access control lists, its owner is the process user, and its list is
    /// protected from inheritance and holds nothing but entries allowing that user. Accounts the
    /// operating system lets pass any permission (root; administrators and backup operators) are not
    /// covered by either rule. NOT_FOUND when nothing is at the path, IO_FAILURE when the entry cannot
    /// be inspected; a file another user made private is IO_FAILURE on Windows (it cannot be opened to
    /// look) and false on POSIX. 'displayPath' is used in messages.
    /// </summary>
    [[nodiscard]] result<bool> is_private_file(const std::filesystem::path &path, std::string_view displayPath);

    /// <summary>
    /// True when a flush refused with 'platformError' means the volume or its driver has no flush, not
    /// that the device failed: ERROR_INVALID_FUNCTION and ERROR_NOT_SUPPORTED on Windows, EINVAL and
    /// ENOTSUP on POSIX. write_file_flushed keeps such a file; every other code fails the write.
    /// </summary>
    [[nodiscard]] bool flush_is_unsupported(u32 platformError) noexcept;

    /// <summary>
    /// How many files write_file_flushed has pushed to their storage device in this process. A
    /// statistic: tests and benchmarks read it to see that a write was durable. It only ever grows.
    /// </summary>
    [[nodiscard]] u64 flushed_file_count() noexcept;

    /// <summary>
    /// Counts one flushed file. Called by write_file_flushed only, right after the flush succeeded.
    /// </summary>
    void note_flushed_file() noexcept;

    /// <summary>
    /// Records that the volume holding 'displayPath' cannot flush: the first call of the process logs
    /// a WARN, later ones are silent, so a project on such a volume does not log on every save. Called
    /// by write_file_flushed only.
    /// </summary>
    void note_unflushable_volume(std::string_view displayPath);

    /// <summary>
    /// True once note_unflushable_volume has logged its WARN in this process; it never becomes false
    /// again. Tests read it to know whether the next report is the first.
    /// </summary>
    [[nodiscard]] bool unflushable_volume_reported() noexcept;

    /// <summary>
    /// Moves 'source' over 'target' so that readers see either the old or the new file, never a
    /// mix. 'target' may or may not exist; an existing file there is replaced even while another
    /// caller holds it open through open_read_handle.
    /// </summary>
    [[nodiscard]] result<void> replace_file(const std::filesystem::path &source, const std::filesystem::path &target);

    /// <summary>
    /// True when replace_file, having been refused a POSIX-semantics rename with 'platformError', takes
    /// the classic MoveFileExW route instead. Windows: true for the Win32 codes with which a volume or its
    /// driver refuses that rename (ERROR_INVALID_FUNCTION from SMB/NAS redirectors and some FAT/exFAT
    /// drivers, ERROR_INVALID_PARAMETER, ERROR_NOT_SUPPORTED). POSIX: always false.
    /// </summary>
    [[nodiscard]] bool rename_falls_back_to_move(u32 platformError) noexcept;

    /// <summary>
    /// The form of a native path the operating system accepts whatever its length. On Windows a path
    /// at or beyond the classic MAX_PATH limit is returned absolute, normalised and prefixed with the
    /// extended-length marker (`\\?\`, `\\?\UNC\` for shares), which every Win32 file call honours;
    /// shorter paths are returned unchanged, and a path already carrying the marker (or the `\\.\` device
    /// form) is normalised to backslashes, because Win32 takes those forms literally. POSIX has no such
    /// limit and returns the path as given.
    /// </summary>
    [[nodiscard]] std::filesystem::path extend_native_path(const std::filesystem::path &path);

    /// <summary>
    /// Like extend_native_path but without the length threshold: on Windows the absolute, normalised,
    /// extended-length form of any path, for directory walks that append components below a short root and
    /// may pass the classic limit on the way down (a walk rooted at the classic form fails there and aborts).
    /// A path already in the extended or device form is normalised as extend_native_path does. POSIX
    /// returns the path unchanged.
    /// </summary>
    [[nodiscard]] std::filesystem::path always_extended_native_path(const std::filesystem::path &path);
}
