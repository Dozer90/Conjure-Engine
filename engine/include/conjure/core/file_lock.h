#pragma once

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <cstdint>
#include <filesystem>

// Cross-process advisory lock on a file. Exclusive, non-blocking.
// Windows: the file is held open with DELETE access and without delete sharing, which alone keeps every other
// file_lock out and stops anyone deleting or renaming the file; LockFileEx over the whole file is added where the
// file system supports it. POSIX: flock(LOCK_EX | LOCK_NB) on an O_CLOEXEC descriptor; anyone may unlink the
// file, so try_acquire and release_and_remove check that the path still names the locked file. Two file_locks
// conflict even inside one process (except where NFS emulates flock with process-scoped locks). No program a child
// process runs gets the lock, however it was spawned: the Windows handle is not inheritable and the POSIX descriptor
// is close-on-exec (only a POSIX fork that does not exec keeps a copy, and with it the lock, until it exits). The
// operating system releases the lock when its holder dies (Windows may take a moment). try_acquire never removes a
// file, whether it created or found it. The path is opened as given: on Windows a path of MAX_PATH characters or
// more must be passed in the extended-length form.
//
// Locks only exclude each other where the file system enforces them: a project open at once from Windows and from
// WSL or Linux (drvfs, 9p), NFS mounted with nolock, and Linux CIFS mounted with nobrl or local-only flock lock
// nothing across those boundaries, and none of that can be detected from here. On a network share, a lock whose
// holder's machine crashed or lost the network stays until the server drops that client's session (SMB
// durable-handle timeout, NFS lease or client reboot).

namespace conjure::core {

    /// <summary>
    /// Whether try_acquire creates the file.
    /// </summary>
    enum class file_lock_open : u8 {
        CREATE,     // create an empty file when missing; an existing file is never truncated
        EXISTING    // NOT_FOUND when missing; nothing is created
    };

    /// <summary>
    /// An exclusive cross-process lock on one file. Move-only; destroying a held lock releases it.
    /// Threading: one object belongs to one thread at a time; distinct objects may be used from any thread.
    /// </summary>
    class CONJURE_API file_lock {
        private:
            static constexpr std::intptr_t INVALID_NATIVE = -1;     // INVALID_HANDLE_VALUE / no descriptor
            std::intptr_t mNative = INVALID_NATIVE;
            std::filesystem::path mPath = {};

        public:
            file_lock();
            ~file_lock();                                           // release()

            file_lock(const file_lock &) = delete;
            file_lock &operator=(const file_lock &) = delete;
            file_lock(file_lock &&other) noexcept;                  // other is left not held, path empty
            file_lock &operator=(file_lock &&other) noexcept;       // releases the lock it replaces

        public:
            /// <summary>
            /// Opens (or, with CREATE, creates) 'path' and tries to lock it without waiting. Success with held() ==
            /// false when another holder has it (Windows: also a foreign handle opened with delete access or whose
            /// share mode refuses reading, writing or deletion, or one holding a byte-range lock in the file).
            /// Fails with INVALID_ARGUMENT for an empty path, one containing NUL or an 'open' value outside
            /// file_lock_open; NOT_FOUND for a missing file under EXISTING, a missing parent folder, or a file
            /// removed while it was being locked; PERMISSION_DENIED when access is refused (Windows: mostly also
            /// for a file whose deletion is pending), the path is a directory, or (POSIX) a symbolic link;
            /// UNSUPPORTED when the file system has no locks (POSIX only; on Windows the share mode alone holds the
            /// file and the result is held); IO_FAILURE when the file kept being replaced under CREATE;
            /// BACKEND_FAILURE otherwise. Messages name the path (when there is one); those from a failed system
            /// call also carry the operating system's text. Never removes a file, on any outcome: a file it created
            /// stays when it then fails.
            /// </summary>
            [[nodiscard]] static result<file_lock> try_acquire(const std::filesystem::path &path, file_lock_open open);

            /// <summary>
            /// True while this object holds the lock.
            /// </summary>
            [[nodiscard]] bool held() const noexcept;

            /// <summary>
            /// The path given to try_acquire; empty when nothing is held.
            /// </summary>
            [[nodiscard]] const std::filesystem::path &path() const noexcept;

            /// <summary>
            /// Releases the lock and closes the file. Safe when nothing is held.
            /// </summary>
            void release() noexcept;

            /// <summary>
            /// Deletes the file while still holding it, then releases. Where Windows cannot delete with POSIX
            /// semantics (before Windows 10 1809, or on a file system without them such as FAT) the name stays,
            /// pending deletion, until every other handle to the file has closed. INVALID_STATE when nothing is held.
            /// NOT_FOUND (POSIX) when the path no longer names the held file, which is left alone. On an OS
            /// failure the lock is released, the file stays and the error names it. Nothing is held afterwards,
            /// whatever the outcome.
            /// </summary>
            [[nodiscard]] result<void> release_and_remove();
    };
}
