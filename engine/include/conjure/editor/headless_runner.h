#pragma once

#include "conjure/editor/editor_options.h"
#include "conjure/editor/protocol/protocol_message.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <filesystem>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

// `conjure_editor --headless --exec commands.json` (docs/design/04_editor.md 3.11): a JSON array of requests
// or NDJSON, one request per line; a missing id becomes the zero-based index. Every request is dispatched,
// frames are pumped until an asynchronous response exists, responses and events are written as NDJSON to the
// output; the run stops at the first failure unless --keep-going. editor.quit ends the run after its reply, and
// its non-zero exitCode becomes the run's exit code. Exit codes: 0 all ok, 1 a command failed, 2 startup or
// project failure (also a project.open in the file whose restart failed for both projects) or responses that
// could not be written, 3 bad arguments (a missing or unparseable file).

namespace conjure::editor {

    class editor_application;

    struct CONJURE_API exec_summary {
        u32 executed = 0;
        u32 succeeded = 0;
        u32 failed = 0;
        i32 firstFailedIndex = -1;
        int exitCode = 0;

        /// <summary>
        /// {executed, succeeded, failed, firstFailedIndex, exitCode}.
        /// </summary>
        [[nodiscard]] core::json::document to_json() const;

        /// <summary>
        /// Counts one answered request: executed, then succeeded or failed; the first failure keeps its index in
        /// firstFailedIndex.
        /// </summary>
        void record(bool ok, u32 index) noexcept;
    };

    class CONJURE_API headless_runner {
        public:
            /// <summary>
            /// The placeholder command files may use for the project root in string arguments.
            /// </summary>
            static constexpr std::string_view PROJECT_PLACEHOLDER = "${project}";

        private:
            u32 mFramesPumped = 0;

        public:
            headless_runner();
            ~headless_runner();

            headless_runner(const headless_runner &) = delete;
            headless_runner &operator=(const headless_runner &) = delete;

        public:
            /// <summary>
            /// Runs options.execFile against the application, writing to options.outputFile or std::cout.
            /// INVALID_ARGS (exit 3) when the file is missing or malformed; IO_ERROR when the output cannot
            /// be opened.
            /// </summary>
            [[nodiscard]] core::result<exec_summary> run(
              editor_application &application,
              const editor_options &options
            );

            /// <summary>
            /// Runs already parsed requests, writing NDJSON lines to 'out'. ${project} expands to the root of the
            /// project open when each request runs. An asynchronous request is waited for until it answers; the
            /// dispatcher's --timeout-seconds answers it with TIMEOUT, and should it never answer, the runner gives
            /// up after twice that plus five seconds and ends the run. editor.quit ends the run after its reply. A
            /// response that cannot be written (the stream failed) ends the run after that request, which is still
            /// counted, with exitCode RUNTIME_FAILURE (2). A non-zero quit code the application holds at the end
            /// (editor.quit's exitCode, or INIT_FAILURE from a failed project restart) becomes exitCode.
            /// </summary>
            [[nodiscard]] exec_summary run_requests(
              editor_application &application,
              std::span<const protocol_request> requests,
              std::ostream &out,
              bool keepGoing
            );

            /// <summary>
            /// Parses a command file: a JSON array of request objects, or NDJSON. A leading UTF-8 byte order
            /// mark is skipped, as are blank lines; a missing id becomes the index. INVALID_ARGS names the
            /// offending line.
            /// </summary>
            [[nodiscard]] static core::result<std::vector<protocol_request>> parse_command_file(std::string_view text);

            /// <summary>
            /// Replaces PROJECT_PLACEHOLDER in every string of the arguments with the project root.
            /// </summary>
            static void expand_placeholders(protocol_request &request, const std::filesystem::path &projectRoot);

            /// <summary>
            /// Frames pumped during the last run.
            /// </summary>
            [[nodiscard]] u32 frames_pumped() const noexcept {
                return mFramesPumped;
            }
    };
}
