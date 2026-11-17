#pragma once

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// Project trust (docs/design/04_editor.md D27, code sweep F187). Opening a project runs code from its folder with
// the rights of the user who runs the editor: the engine loads its native modules and its game assembly while it
// starts, and script.compile, module.build and play.start run more of it afterwards. The editor therefore opens a
// project only once somebody answered for it, and project_trust is where the answers are kept:
//
//   - for the session: in memory, gone when the editor exits (--trust-project, project.trust {remember: false},
//     and every answer while there is no store file or it cannot be written);
//   - remembered: in the trust store, one JSON file per user of this machine and outside every project folder, so
//     that a project cannot ship its own answer (default_trust_store_file()).
//
// A project is identified by the resolved absolute path of its root folder (project_key). Trust belongs to the
// path, not to the files: what is later put into a trusted folder is trusted with it, and a project that is moved
// or copied is asked about again. The gate itself is editor_application (--project, project.open); this type only
// answers "was this project trusted" and records answers. Main thread only.
//
// The store file is written through core::vfs::write_private_file and is read only while
// core::vfs::is_private_file says that no other account could have written it; a file that fails the check, does
// not parse, is larger than MAX_STORE_BYTES or was written by a newer editor counts as empty (one WARN per read)
// and is replaced by the next remembered answer. The limits:
//   - The check and the read are two steps on the path, not one on an open file: a file swapped in between them
//     is believed. So the folder of the store must not be writable by other accounts. The default folder is not;
//     a folder named with --trust-store is the caller's to choose.
//   - Two editors that answer at the same moment read and write the file one after the other without a lock: one
//     of the two answers can be lost from the store, and that project is asked about again.
//   - The file's protection is that of write_private_file (privileged accounts can open it; on a volume without
//     owner-only files nothing is remembered).

namespace conjure::editor {

    /// <summary>
    /// What is known about a project. Ordered: a greater value is the longer-lived answer.
    /// </summary>
    enum class trust_state : u8 {
        UNTRUSTED = 0,          // nobody answered for the project: the editor does not open it
        SESSION,                // trusted until this editor exits
        REMEMBERED              // trusted in the trust store of this user on this machine
    };

    /// <summary>
    /// The wire spelling of a state: "untrusted", "session" or "remembered"; "" for a value outside the
    /// enumeration.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(trust_state state) noexcept;

    /// <summary>
    /// One remembered answer, as the trust store holds it.
    /// </summary>
    struct trusted_project {
        std::string path = "";              // project_key() of the project root (UTF-8, forward slashes)
        std::string trustedAt = "";         // UTC, "2026-11-17T09:30:00Z"
        std::string origin = "";            // who answered: "editor", "created", or a transport name ("tcp")

        [[nodiscard]] bool operator==(const trusted_project &) const = default;
    };

    class CONJURE_API project_trust {
        public:
            static constexpr std::string_view STORE_FILE_NAME = "trusted_projects.json";
            static constexpr u32 STORE_SCHEMA_VERSION = 1;

            /// <summary>
            /// The store keeps the most recent answers: one more than this drops the oldest, and that project
            /// is asked about again.
            /// </summary>
            static constexpr usize MAX_REMEMBERED_PROJECTS = 1024;

            /// <summary>
            /// A store file larger than this is not read, and none is written: an answer that would take
            /// the store past it holds for the session instead. Paths of ordinary length stay far below it
            /// (MAX_REMEMBERED_PROJECTS entries of 4 KiB each would not).
            /// </summary>
            static constexpr usize MAX_STORE_BYTES = 4 * MIB;

            /// <summary>
            /// An origin text is cut to this many bytes before it is stored.
            /// </summary>
            static constexpr usize MAX_ORIGIN_BYTES = 32;

        private:
            std::filesystem::path mStoreFile = {};          // empty = nothing is remembered
            std::vector<std::string> mSession = {};         // project keys trusted until the editor exits

        public:
            project_trust();
            ~project_trust();

            project_trust(const project_trust &) = delete;
            project_trust &operator=(const project_trust &) = delete;

        public:
            /// <summary>
            /// Names the trust store file; an empty path (the default) means that nothing is remembered and
            /// every answer is for the session. Nothing is read or created by this call. The session answers
            /// are kept.
            /// </summary>
            void set_store_file(std::filesystem::path file);

            [[nodiscard]] const std::filesystem::path &store_file() const noexcept {
                return mStoreFile;
            }

            /// <summary>
            /// The text a project is remembered under: its root folder made absolute against the working
            /// directory, with symbolic links, "." and ".." resolved by the file system as far as the path
            /// exists, in UTF-8 with forward slashes and no trailing separator. Two spellings the operating
            /// system resolves to the same text are one project; two it does not (another letter case on a
            /// volume that ignores case but does not report the stored one) are two, and the second is asked
            /// about again. INVALID_ARGUMENT for an empty path and for one that is not valid UTF-8, decided
            /// from the path as given before the file system is asked, so the answer is the same on every
            /// volume, and again from the resolved text (the working directory, the target of a link).
            /// IO_FAILURE when the path cannot be resolved.
            /// </summary>
            [[nodiscard]] static core::result<std::string> project_key(const std::filesystem::path &projectRoot);

            /// <summary>
            /// What is known about the project: REMEMBERED when the store names it, SESSION when it was
            /// trusted for this session, UNTRUSTED otherwise and for a path project_key() refuses. The store
            /// file is read on every call, so an answer another editor of this user recorded or revoked is
            /// seen at once.
            /// </summary>
            [[nodiscard]] trust_state state_of(const std::filesystem::path &projectRoot) const;

            /// <summary>
            /// True unless state_of() is UNTRUSTED.
            /// </summary>
            [[nodiscard]] bool is_trusted(const std::filesystem::path &projectRoot) const;

            /// <summary>
            /// Records that the project is trusted and returns the state it now has. With 'remember' and a
            /// store file the answer is written to the store (the entry of the same project is replaced, the
            /// oldest entry dropped beyond MAX_REMEMBERED_PROJECTS; the folder of the store is created). When
            /// that is not asked for, there is no store file, the store cannot be written or would grow past
            /// MAX_STORE_BYTES, or the store does not show the answer right after it was written (another
            /// editor of this user wrote the file in between), the answer holds for the session, with one
            /// WARN that names why in the last three cases: a "yes" this call returned is never UNTRUSTED,
            /// only not remembered. What this call cannot do is keep a remembered answer in the store
            /// afterwards: the other editor's write can still drop it, and the project is then asked about
            /// again.
            /// 'origin' says who answered and is stored with the entry, cut to MAX_ORIGIN_BYTES. Whether the
            /// folder holds a project is the caller's question (the project.trust command asks it). Fails as
            /// project_key() does, and then records nothing.
            /// </summary>
            [[nodiscard]] core::result<trust_state> trust(
              const std::filesystem::path &projectRoot,
              bool remember,
              std::string_view origin
            );

            /// <summary>
            /// Forgets the project: removes it from the session answers and from the store, and returns
            /// whether either held it. The folder need not exist any more. IO_FAILURE, with the session
            /// answer already removed, when the store names the project and cannot be rewritten: the project
            /// is then still trusted on disk. A project that is open stays open; the answer counts the next
            /// time it is opened.
            /// </summary>
            [[nodiscard]] core::result<bool> revoke(const std::filesystem::path &projectRoot);

            /// <summary>
            /// The answers the store holds, oldest first; empty without a store file and for a store that
            /// cannot be used (see the file comment).
            /// </summary>
            [[nodiscard]] std::vector<trusted_project> remembered() const;

        private:
            [[nodiscard]] trust_state state_of_key(const std::string &key) const;
            [[nodiscard]] core::result<std::vector<trusted_project>> read_store() const;
            [[nodiscard]] core::result<void> write_store(const std::vector<trusted_project> &projects) const;
            void trust_for_session(const std::string &key);
    };

    /// <summary>
    /// The trust store conjure_editor uses when --trust-store names none: STORE_FILE_NAME in the folder the
    /// operating system gives a program for the state it keeps per user on one machine. Windows:
    /// %LOCALAPPDATA%/Conjure (the known folder, which does not roam to other machines). Linux:
    /// $XDG_STATE_HOME/conjure when that variable holds an absolute path, otherwise $HOME/.local/state/conjure.
    /// macOS: $HOME/Library/Application Support/Conjure. NOT_FOUND when the system names no such folder (no
    /// known folder; neither variable holds an absolute path): nothing is remembered then. The folder is not
    /// created by this call. Only the executable calls it: an editor_application given no store file remembers
    /// nothing, so tests and embedders never touch the user's folder by accident.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<std::filesystem::path> default_trust_store_file();
}
