#pragma once

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/process.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// Building managed projects at runtime (docs/design/03_scripting.md 3.10). managed_build_runner spawns
// `dotnet build` through core::process on a job, parses the MSBuild diagnostics and posts the report to
// the main thread with core::jobs::post_to_main_thread; the caller drains that queue (the engine does so
// before FRAME_BEGIN) and the editor shows the diagnostics. Lives here so conjure_editor --headless can
// rebuild too.

namespace conjure::scripting {

    /// <summary>
    /// One `path(line,col): error|warning CSxxxx: message` line of the build output.
    /// </summary>
    struct build_diagnostic {
        std::string file = {};
        u32 line = 0;
        u32 column = 0;
        bool isError = false;
        std::string code = {};          // "CS0103"
        std::string message = {};

        [[nodiscard]] bool operator==(const build_diagnostic &) const = default;
    };

    struct build_report {
        bool succeeded = false;
        bool timedOut = false;
        i32 exitCode = 0;
        std::vector<build_diagnostic> diagnostics = {};
        std::string output = {};        // the merged stdout/stderr text
        f64 elapsedSeconds = 0.0;
        core::result<void> launch = core::result<void>::success();   // failure when dotnet could not be started

        /// <summary>
        /// Diagnostics with isError set.
        /// </summary>
        [[nodiscard]] u32 error_count() const noexcept;

        [[nodiscard]] u32 warning_count() const noexcept;
    };

    /// <summary>
    /// Everything a build needs.
    /// </summary>
    struct build_request {
        std::filesystem::path projectPath = {};                             // .csproj
        std::string configuration = "Debug";                                // -c
        std::filesystem::path outputDirectory = {};                         // -o when set
        std::vector<std::pair<std::string, std::string>> properties = {};   // -p:Name=Value
        f64 timeoutSeconds = 600.0;
    };

    class managed_build_runner {
        private:
            struct shared_state {
                std::atomic<bool> building = false;
            };

            core::jobs::job_system *mJobs = nullptr;
            std::shared_ptr<shared_state> mState = std::make_shared<shared_state>();
            core::jobs::job_handle mJob = {};
            std::string mDotnetExecutable = "dotnet";
            u32 mBuildCount = 0;

        public:
            /// <summary>
            /// 'jobs' runs the build off the main thread; nullptr runs it inline inside build() (the report
            /// is still posted to the main-thread queue so callers behave the same either way).
            /// </summary>
            // Not explicit: `managed_build_runner runner = {};` is the common form.
            managed_build_runner(core::jobs::job_system *jobs = nullptr);
            ~managed_build_runner();

            managed_build_runner(const managed_build_runner &) = delete;
            managed_build_runner &operator=(const managed_build_runner &) = delete;

        public:
            /// <summary>
            /// Spawns `dotnet build <project> -c <configuration>` on a job; onComplete runs on the main thread
            /// at the next drain_main_thread_queue(). Asserts and ignores the call while a build runs.
            /// </summary>
            void build(
              const std::filesystem::path &projectPath,
              std::string_view configuration,
              std::function<void(const build_report &)> onComplete
            );

            /// <summary>
            /// Same with every option of build_request.
            /// </summary>
            void build(const build_request &request, std::function<void(const build_report &)> onComplete);

            [[nodiscard]] bool is_building() const noexcept;

            /// <summary>
            /// Blocks until the running build (if any) finished; the report is then in the main-thread queue.
            /// </summary>
            void wait();

            /// <summary>
            /// Builds issued so far.
            /// </summary>
            [[nodiscard]] u32 build_count() const noexcept {
                return mBuildCount;
            }

            /// <summary>
            /// The dotnet program to spawn ("dotnet" from PATH by default).
            /// </summary>
            void set_dotnet_executable(std::string executable);

            [[nodiscard]] const std::string &dotnet_executable() const noexcept {
                return mDotnetExecutable;
            }

            /// <summary>
            /// Parses MSBuild output: `file(line,col): error CS1002: message` and the `warning` form; other
            /// lines are ignored. Paths with spaces and project suffixes `[Game.csproj]` are handled.
            /// </summary>
            [[nodiscard]] static std::vector<build_diagnostic> parse_output(std::string_view msbuildOutput);

            /// <summary>
            /// The process description a request turns into: dotnet build <project> -c <configuration>
            /// [-o <output>] --nologo -clp:NoSummary -p:ContinuousIntegrationBuild=true [-p:Name=Value...].
            /// </summary>
            [[nodiscard]] static core::process_desc command_line(const build_request &request, std::string_view dotnet);

            /// <summary>
            /// Runs a request to completion on the calling thread.
            /// </summary>
            [[nodiscard]] static build_report run_blocking(const build_request &request, std::string_view dotnet);
    };
}
