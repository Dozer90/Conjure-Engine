#pragma once

#include "conjure/core/api.h"
#include "conjure/core/enum_flags.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The virtual file system interface.
//
// A file_system is one backend (a directory on disk, an in-memory tree, a mount table). Paths are
// UTF-8, '/'-separated and relative to the backend root; "" names the root itself. Every backend
// normalises its input (see path_utils::normalize), accepts backslashes and a leading '/', and
// rejects paths that escape the root ("../x", "C:/x") with PERMISSION_DENIED.
//
// Names of new entries are portable. Wherever a call creates an entry (write_file, write_file_atomic,
// write_text_file, create_directories and the destination of rename) every segment of the path must
// pass path_utils::is_portable_name, on every platform and whether or not the segment exists already:
// no control character and none of : * ? " < > |, no trailing dot or space, no Windows device name
// (CON, PRN, AUX, NUL, COM1 to COM9, LPT1 to LPT9, in any case and with any extension). Anything else
// is INVALID_ARGUMENT and nothing is written, so a project made on one platform opens on the others
// and Windows never files "notes.txt:draft" as a hidden stream of notes.txt or "report." as "report".
// read_file, read_text_file, open_read, stat, exists, enumerate, remove, remove_all and the source of
// rename judge no name: an entry that already carries such a name can be read, moved to a portable
// name and removed.
//
// Error codes are part of the contract and identical for every backend:
//   NOT_FOUND          the path (or the source of a rename) does not exist
//   INVALID_ARGUMENT   a directory where a file is required (or vice versa), the root where a
//                      removable entry is required, renaming a directory into itself, a name that
//                      is not portable where an entry is created
//   ALREADY_EXISTS     a file occupies the path given to create_directories, or a directory
//                      occupies the destination of a rename
//   INVALID_STATE      remove() on a directory that is not empty
//   PERMISSION_DENIED  the backend is read-only or the path escapes the root
//   OUT_OF_RANGE       file::read_at with an offset past the end of the file
//   IO_FAILURE         everything the operating system refused; the message carries its text

namespace conjure::core::vfs {

    /// <summary>
    /// What a backend knows about one entry.
    /// </summary>
    struct file_stat {
        u64 size = 0;                       // bytes; 0 for directories
        u64 writeTimeNanoseconds = 0;       // last modification; comparable within one backend
        bool isDirectory = false;
        bool isRegular = false;
    };

    /// <summary>
    /// Controls file_system::enumerate. When neither FILES nor DIRECTORIES is set, both kinds are
    /// reported, so NONE lists the direct children of a directory and RECURSIVE its whole subtree.
    /// </summary>
    enum class enumerate_flags : u8 {
        NONE = 0,
        RECURSIVE = 1 << 0,
        FILES = 1 << 1,
        DIRECTORIES = 1 << 2,
        ALL = FILES | DIRECTORIES
    };
    CONJURE_ENUM_FLAGS(enumerate_flags);

    /// <summary>
    /// Called once per enumerated entry with the backend-relative path of the entry.
    /// </summary>
    using enumerate_callback = std::function<void(std::string_view path, const file_stat &stat)>;

    /// <summary>
    /// A file opened for random-access reads. The size is fixed at open time; read_at may be
    /// called from several threads at once. An open handle never blocks other callers: the file
    /// can be rewritten, replaced, renamed over or removed meanwhile on every backend.
    /// </summary>
    class CONJURE_API file {
        public:
            file();
            virtual ~file();

            file(const file &) = delete;
            file &operator=(const file &) = delete;

        public:
            /// <summary>
            /// Reads up to out.size() bytes starting at 'offset' and returns how many were read,
            /// which is fewer than requested only at the end of the file. An offset past the end
            /// is OUT_OF_RANGE; an offset equal to the size reads zero bytes.
            /// </summary>
            [[nodiscard]] virtual result<usize> read_at(u64 offset, std::span<std::byte> out) = 0;

            /// <summary>
            /// Size of the file in bytes when it was opened.
            /// </summary>
            [[nodiscard]] virtual u64 size() const noexcept = 0;
    };

    /// <summary>
    /// One storage backend. See the header comment for the path and error contract.
    /// </summary>
    class CONJURE_API file_system {
        public:
            file_system();
            virtual ~file_system();

            file_system(const file_system &) = delete;
            file_system &operator=(const file_system &) = delete;

        public:
            /// <summary>
            /// Reads a whole file. NOT_FOUND when missing, INVALID_ARGUMENT for a directory.
            /// </summary>
            [[nodiscard]] virtual result<std::vector<std::byte>> read_file(std::string_view path) = 0;

            /// <summary>
            /// Reads a whole file as text. A leading UTF-8 byte order mark is dropped; nothing
            /// else is transcoded. Implemented on top of read_file.
            /// </summary>
            [[nodiscard]] virtual result<std::string> read_text_file(std::string_view path);

            /// <summary>
            /// Creates or truncates a file and writes 'data'. Missing parent directories are
            /// created. INVALID_ARGUMENT when a directory occupies the path or a segment of it is
            /// not a portable name (header comment), IO_FAILURE when a file occupies one of the
            /// parents.
            /// </summary>
            [[nodiscard]] virtual result<void> write_file(std::string_view path, std::span<const std::byte> data) = 0;

            /// <summary>
            /// Like write_file, but the path never holds a partial file, for a concurrent reader
            /// or after a crash: the data goes to a temporary sibling ("<path>.tmp-<uuid>") that
            /// then replaces the target in one step. A backend that stores on a device flushes the
            /// temporary to it before the swap, so after a power cut the path holds the old
            /// contents or the new ones, never a torn file (the swap itself may be lost, which
            /// leaves the old contents and possibly the temporary). Two limits: a volume whose
            /// driver has no flush at all still takes the write, without this guarantee, and logs
            /// one warning per process; on macOS the flush is fsync, which hands the data to the
            /// drive without waiting for the drive's own write cache, so the guarantee there is
            /// only as strong as that cache. That flush costs one device round trip per call,
            /// typically milliseconds: use write_file for output that can be regenerated.
            /// </summary>
            [[nodiscard]] virtual result<void> write_file_atomic(
              std::string_view path,
              std::span<const std::byte> data
            ) = 0;

            /// <summary>
            /// Writes UTF-8 text with write_file.
            /// </summary>
            [[nodiscard]] result<void> write_text_file(std::string_view path, std::string_view text);

            /// <summary>
            /// Describes an entry. "" describes the root. NOT_FOUND when missing.
            /// </summary>
            [[nodiscard]] virtual result<file_stat> stat(std::string_view path) = 0;

            /// <summary>
            /// True when stat succeeds.
            /// </summary>
            [[nodiscard]] virtual bool exists(std::string_view path);

            /// <summary>
            /// Lists the entries of a directory in ascending path order, each as a path relative
            /// to the backend root ("dir/sub/file.txt"). NOT_FOUND when the directory is missing,
            /// INVALID_ARGUMENT when the path names a file.
            /// </summary>
            [[nodiscard]] virtual result<void> enumerate(
              std::string_view directory,
              enumerate_flags flags,
              const enumerate_callback &callback
            ) = 0;

            /// <summary>
            /// Moves a file or directory. An existing file at 'to' is replaced (even while a
            /// reader holds it open); an existing directory there is ALREADY_EXISTS. The parent of
            /// 'to' must exist (NOT_FOUND). Renaming an entry onto itself succeeds and changes
            /// nothing; renaming it onto another spelling of itself on a case-insensitive volume
            /// changes the case. 'to' is a name this call creates, so it must be portable (header
            /// comment, INVALID_ARGUMENT, checked before 'from' is looked up); 'from' may be any
            /// name that exists.
            /// </summary>
            [[nodiscard]] virtual result<void> rename(std::string_view from, std::string_view to) = 0;

            /// <summary>
            /// Deletes a file or an empty directory. INVALID_STATE for a non-empty directory,
            /// INVALID_ARGUMENT for the root, NOT_FOUND when missing.
            /// </summary>
            [[nodiscard]] virtual result<void> remove(std::string_view path) = 0;

            /// <summary>
            /// Deletes a file or a directory with everything inside it. INVALID_ARGUMENT for the
            /// root, NOT_FOUND when missing. The default walks the tree with enumerate and remove.
            /// </summary>
            [[nodiscard]] virtual result<void> remove_all(std::string_view path);

            /// <summary>
            /// Creates a directory and every missing parent; an existing directory is not an
            /// error. ALREADY_EXISTS when a file occupies the path, IO_FAILURE when a file
            /// occupies one of the parents, INVALID_ARGUMENT when a segment of the path is not a
            /// portable name (header comment).
            /// </summary>
            [[nodiscard]] virtual result<void> create_directories(std::string_view path) = 0;

            /// <summary>
            /// Opens a file for random-access reads. NOT_FOUND when missing, INVALID_ARGUMENT for
            /// a directory.
            /// </summary>
            [[nodiscard]] virtual result<std::unique_ptr<file>> open_read(std::string_view path) = 0;

            /// <summary>
            /// True when every mutating operation fails with PERMISSION_DENIED.
            /// </summary>
            [[nodiscard]] virtual bool is_read_only() const noexcept;
    };
}
