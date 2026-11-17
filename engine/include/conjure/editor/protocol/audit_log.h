#pragma once

#include "conjure/editor/editor_error.h"
#include "conjure/editor/protocol/access_token.h"
#include "conjure/editor/protocol/protocol_transport.h"

#include "conjure/core/api.h"
#include "conjure/core/json/value.h"
#include "conjure/core/types.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>

// The audit log of the AI control protocol (docs/design/04_editor.md 3.12, D26, code sweep F187): one line
// of JSON per command a transport client had dispatched, appended to <project>/.conjure/ai_audit.log. The
// protocol server owns the log and decides what is recorded; this class formats a record, keeps tokens out
// of it and keeps the files bounded. Main thread only.
//
//   {"time":"2026-11-17T09:15:02Z","clientId":3,"transport":"tcp","access":"inspect","cmd":"entity.get",
//    "args":{"entity":"Player"},"ok":true}
//
// What a record never holds. The value of a member named "token", at any depth of the arguments of any
// command, is written as "[withheld]": that covers session.hello and a hello sent under a mistyped command
// name. Wherever else a text of the request (the command name, a key, a string value) contains one of the
// secrets handed to record(), that part is written as "[token]", and arguments whose serialised form still
// contains a secret (a number equal to an all-digit token) are written as "[withheld]" as a whole. The
// limit of this: the record's own words (its field names, the markers) are not client text, so a token
// that is part of them, such as a one-letter --ai-token, cannot be kept out of the line. The secrets are
// looked for with the constant_time_ functions of access_token.h, never with a substring search, whose
// duration would tell a client that may only look how much of the other token its text had right.
//
// How the size is bounded. A line is at most MAX_LINE_BYTES: the command name is cut to
// MAX_COMMAND_BYTES, a string value or key to MAX_TEXT_BYTES, an array to its first MAX_ARRAY_ITEMS items,
// an object to its first MAX_OBJECT_MEMBERS members, nesting to MAX_DEPTH levels, and arguments that still
// serialise to more than MAX_ARGS_BYTES are replaced by a note that says so. A line that would take the
// file past max_file_bytes() starts a new file: the current one becomes ai_audit.1.log, replacing the
// previous one of that name; when that move is refused (a folder has the name, or a program holds a file
// open in a way that forbids it) the full file is emptied in place instead and its records are lost. So
// the two files together never exceed twice max_file_bytes(), the log covers the most recent commands
// only, and a client that sends enough requests pushes older records out. A file found larger than the
// limit when the log opens (written under a larger limit, or not by the editor) is removed, not kept. The
// bound is per editor: a second editor on the same project appends to the same files and counts only its
// own lines, so with two editors a file can hold up to twice its limit.
//
// How the work is bounded. A record costs the same small amount whatever the request it is made from: a
// text is read only as far as its kept part needs, and the copy of the arguments stops as soon as what it
// has kept is known to exceed MAX_ARGS_BYTES. A request of the largest size a line may have (16 MiB), made
// of one long string, a hundred thousand members or a million items, is recorded as fast as a small one.
//
// Who can read it. The file is created through core::vfs::write_private_file, so that only the user
// running the editor can open it, with that call's limits. A regular file that is already there is appended
// to only while core::vfs::is_private_file holds for it; otherwise (another account made it readable by
// others, or it is a hard link to such a file) an owner-only file takes its place and its records are lost.
// On a volume that keeps no owner-only files (FAT, exFAT) the file is an ordinary one, and one that is
// already there is appended to with the permissions it has. A symbolic link at the path is replaced, never
// written through; the check and the open are two steps, so this stops a link that was planted beforehand, not
// one swapped in between them.
// The log is a record for the user, not evidence: a full-control client can delete it like any other file.

namespace conjure::editor {

    /// <summary>
    /// One dispatched command as the audit log records it. The views must outlive the record() call only.
    /// </summary>
    struct audit_entry {
        std::string_view command = "";                      // as the client sent it; unknown names included
        core::json::value args = {};                        // the request's arguments; invalid = none
        u32 clientId = 0;
        transport_kind transport = transport_kind::LOCAL;
        access_level access = access_level::NONE;           // what the client held when the command ran
        protocol_error_code error = protocol_error_code::NONE;  // why it failed; read only when !ok
        bool ok = true;
        bool async = false;                                 // accepted and still running: no outcome yet
        bool batchChild = false;                            // a child of an editor.batch
    };

    class CONJURE_API audit_log {
        public:
            static constexpr std::string_view FILE_NAME = "ai_audit.log";
            static constexpr std::string_view PREVIOUS_FILE_NAME = "ai_audit.1.log";
            static constexpr std::string_view WITHHELD = "[withheld]";
            static constexpr std::string_view TOKEN_MARK = "[token]";
            static constexpr usize DEFAULT_MAX_FILE_BYTES = 4 * MIB;
            static constexpr usize MAX_LINE_BYTES = 4 * KIB;
            static constexpr usize MAX_ARGS_BYTES = 2 * KIB;
            static constexpr usize MAX_COMMAND_BYTES = 128;
            static constexpr usize MAX_TEXT_BYTES = 160;
            static constexpr usize MAX_ARRAY_ITEMS = 16;
            static constexpr usize MAX_OBJECT_MEMBERS = 32;
            static constexpr usize MAX_DEPTH = 6;

        private:
            std::filesystem::path mDirectory = {};
            std::ofstream mStream = {};
            usize mSize = 0;                                // bytes in the current file, as this log counts them
            usize mMaxFileBytes = DEFAULT_MAX_FILE_BYTES;
            u64 mRecordedCount = 0;
            bool mFailed = false;                           // the files of mDirectory cannot be written: reported once

        public:
            audit_log();
            ~audit_log();

            audit_log(const audit_log &) = delete;
            audit_log &operator=(const audit_log &) = delete;

        public:
            /// <summary>
            /// Names the folder the log lives in (&lt;project&gt;/.conjure) and closes the files of the
            /// previous one. An empty path means there is nowhere to write: record() then does nothing.
            /// Nothing is created before the first record.
            /// </summary>
            void set_directory(const std::filesystem::path &directory);

            /// <summary>
            /// The folder given to set_directory (empty when none).
            /// </summary>
            [[nodiscard]] const std::filesystem::path &directory() const noexcept {
                return mDirectory;
            }

            /// <summary>
            /// The file records are appended to, directory() / FILE_NAME; empty without a folder.
            /// </summary>
            [[nodiscard]] std::filesystem::path file() const;

            /// <summary>
            /// The file the current one becomes when it is full, directory() / PREVIOUS_FILE_NAME; empty
            /// without a folder.
            /// </summary>
            [[nodiscard]] std::filesystem::path previous_file() const;

            /// <summary>
            /// The size a file is allowed to reach (DEFAULT_MAX_FILE_BYTES unless changed). A value below
            /// MAX_LINE_BYTES is raised to it, so that every line fits a file. The log is closed; the next
            /// record opens it under the new limit, which removes a file that is larger.
            /// </summary>
            void set_max_file_bytes(usize bytes);

            [[nodiscard]] usize max_file_bytes() const noexcept {
                return mMaxFileBytes;
            }

            /// <summary>
            /// Appends one record, as format_line writes it with the current time. 'secrets' are the
            /// tokens to keep out of it; empty ones are ignored. The first record of a folder creates the
            /// folder and the file. When the file cannot be created or written, one WARN says so and
            /// nothing more is recorded until set_directory is called again; the editor carries on.
            /// </summary>
            void record(const audit_entry &entry, std::span<const std::string_view> secrets);

            /// <summary>
            /// Hands the lines recorded so far to the operating system. The server calls it once per
            /// frame, so a crash loses the records of at most the frame it happened in.
            /// </summary>
            void flush();

            /// <summary>
            /// Flushes and closes the file. The folder stays set: the next record reopens the file.
            /// </summary>
            void close();

            /// <summary>
            /// True while the file is open, that is, after a record and before close() or set_directory().
            /// </summary>
            [[nodiscard]] bool is_open() const noexcept {
                return mStream.is_open();
            }

            /// <summary>
            /// Lines written since construction, whatever the folder.
            /// </summary>
            [[nodiscard]] u64 recorded_count() const noexcept {
                return mRecordedCount;
            }

            /// <summary>
            /// The record of 'entry' as one line of JSON without the line end, at most MAX_LINE_BYTES
            /// long: {"time", "clientId", "transport", "access", "cmd", "args", "ok"}, then "error" (the
            /// code's name) when the command failed, "async": true when it was accepted and is still
            /// running, and "batch": true for a child of an editor.batch. The tokens in 'secrets' and
            /// every member named "token" are kept out as the top of this header describes.
            /// </summary>
            [[nodiscard]] static std::string format_line(
              const audit_entry &entry,
              std::span<const std::string_view> secrets,
              std::chrono::system_clock::time_point time
            );

            /// <summary>
            /// A point in time as the log writes it: UTC, to the second, "2026-11-17T09:15:02Z".
            /// </summary>
            [[nodiscard]] static std::string format_time(std::chrono::system_clock::time_point time);

        private:
            [[nodiscard]] bool open();
            [[nodiscard]] bool start_file(const std::filesystem::path &target);
            [[nodiscard]] bool rotate();
            void fail(std::string_view reason);
    };
}
