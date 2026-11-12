#include "conjure/scripting/managed_build.h"

#include "scripting/scripting_internal.h"

#include "conjure/scripting/log_category.h"
#include "conjure/scripting/scripting_error.h"

#include "conjure/core/assert.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/log.h"
#include "conjure/core/string_utils.h"

#include <algorithm>
#include <optional>
#include <utility>

using namespace conjure;
using namespace conjure::scripting;

namespace {

    /// <summary>
    /// Parses one MSBuild line: `<path>(<line>,<col>): error|warning <CODE>: <message> [<project>]`. The
    /// path may contain spaces and drive colons; the location may also be `(<line>)` alone; the trailing
    /// `[project.csproj]` is removed from the message.
    /// </summary>
    [[nodiscard]] std::optional<build_diagnostic> parse_line(std::string_view line) {
        const std::string_view trimmed = core::str::trim(line);
        usize keyword = std::string_view::npos;
        bool isError = false;
        const usize errorAt = trimmed.find(": error ");
        const usize warningAt = trimmed.find(": warning ");
        if (errorAt != std::string_view::npos && (warningAt == std::string_view::npos || errorAt < warningAt)) {
            keyword = errorAt;
            isError = true;
        } else if (warningAt != std::string_view::npos) {
            keyword = warningAt;
        }
        if (keyword == std::string_view::npos) {
            return std::nullopt;
        }

        build_diagnostic diagnostic = {};
        diagnostic.isError = isError;

        // Location: "path(line,col)" or "path(line)" or a bare path / tool name.
        const std::string_view location = core::str::trim(trimmed.substr(0, keyword));    // "MSBUILD : error"
        const usize open = location.rfind('(');
        if (open != std::string_view::npos && location.ends_with(')')) {
            const std::string_view numbers = location.substr(open + 1, location.size() - open - 2);
            const usize comma = numbers.find(',');
            const std::string_view lineText = comma == std::string_view::npos ? numbers : numbers.substr(0, comma);
            const std::optional<u32> lineNumber = core::str::parse_integer<u32>(core::str::trim(lineText));
            if (lineNumber.has_value()) {
                diagnostic.file = std::string(location.substr(0, open));
                diagnostic.line = *lineNumber;
                if (comma != std::string_view::npos) {
                    const std::string_view columnText = core::str::trim(numbers.substr(comma + 1));
                    diagnostic.column = core::str::parse_integer<u32>(columnText).value_or(0);
                }
            } else {
                diagnostic.file = std::string(location);
            }
        } else {
            diagnostic.file = std::string(location);
        }

        // Code and message: "CS1002: message [project]".
        std::string_view rest = trimmed.substr(keyword + (isError ? 8 : 10));
        const usize colon = rest.find(':');
        if (colon == std::string_view::npos) {
            return std::nullopt;
        }
        diagnostic.code = std::string(core::str::trim(rest.substr(0, colon)));
        std::string_view message = core::str::trim(rest.substr(colon + 1));
        if (message.ends_with(']')) {
            const usize bracket = message.rfind('[');
            if (bracket != std::string_view::npos) {
                message = core::str::trim(message.substr(0, bracket));
            }
        }
        diagnostic.message = std::string(message);
        return diagnostic;
    }
}


/*===========================================================================*/
/*===========================================================================*/
// build_report

u32 build_report::error_count() const noexcept {
    u32 count = 0;
    for (const build_diagnostic &diagnostic : diagnostics) {
        if (diagnostic.isError) {
            ++count;
        }
    }
    return count;
}

u32 build_report::warning_count() const noexcept {
    return static_cast<u32>(diagnostics.size()) - error_count();
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

managed_build_runner::managed_build_runner(core::jobs::job_system *jobs) :
    mJobs(jobs) {}

managed_build_runner::~managed_build_runner() {
    wait();
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

void managed_build_runner::build(
  const std::filesystem::path &projectPath,
  std::string_view configuration,
  std::function<void(const build_report &)> onComplete
) {
    build_request request = {};
    request.projectPath = projectPath;
    request.configuration = std::string(configuration);
    build(request, std::move(onComplete));
}

void managed_build_runner::build(const build_request &request, std::function<void(const build_report &)> onComplete) {
    CONJURE_ASSERT(!is_building(), "managed_build_runner::build while a build runs");
    if (is_building()) {
        return;
    }
    ++mBuildCount;
    mState->building.store(true, std::memory_order_release);

    const std::shared_ptr<shared_state> state = mState;
    const std::string dotnet = mDotnetExecutable;
    const build_request copy = request;
    auto work = [state, dotnet, copy, onComplete = std::move(onComplete)]() mutable {
        build_report report = run_blocking(copy, dotnet);
        // The callback runs on the main thread; the flag drops only once the report is queued so a caller
        // that waits and drains always sees it.
        core::jobs::post_to_main_thread([onComplete = std::move(onComplete), report = std::move(report)]() {
            if (onComplete) {
                onComplete(report);
            }
        });
        state->building.store(false, std::memory_order_release);
    };

    if (mJobs == nullptr || !mJobs->is_running()) {
        work();
        return;
    }
    mJob = mJobs->schedule(std::move(work), core::jobs::job_priority::LOW);
}

bool managed_build_runner::is_building() const noexcept {
    return mState->building.load(std::memory_order_acquire);
}

void managed_build_runner::wait() {
    if (mJobs != nullptr && mJob.valid()) {
        mJobs->wait(mJob);
        mJob.reset();
    }
}

void managed_build_runner::set_dotnet_executable(std::string executable) {
    mDotnetExecutable = std::move(executable);
}

std::vector<build_diagnostic> managed_build_runner::parse_output(std::string_view msbuildOutput) {
    std::vector<build_diagnostic> diagnostics = {};
    for (const std::string_view line : core::str::split_lines(msbuildOutput)) {
        std::optional<build_diagnostic> diagnostic = parse_line(line);
        if (!diagnostic.has_value()) {
            continue;
        }
        // MSBuild repeats every diagnostic once per project in the summary; keep one.
        const bool seen = std::find(diagnostics.begin(), diagnostics.end(), *diagnostic) != diagnostics.end();
        if (!seen) {
            diagnostics.push_back(std::move(*diagnostic));
        }
    }
    return diagnostics;
}

core::process_desc managed_build_runner::command_line(const build_request &request, std::string_view dotnet) {
    core::process_desc desc = {};
    desc.executable = std::string(dotnet);
    // core::process takes UTF-8 arguments; path::string() would hand it the ANSI code page on Windows.
    desc.arguments = {"build", detail::path_to_utf8(request.projectPath), "-c", request.configuration};
    if (!request.outputDirectory.empty()) {
        desc.arguments.push_back("-o");
        desc.arguments.push_back(detail::path_to_utf8(request.outputDirectory));
    }
    desc.arguments.push_back("--nologo");
    desc.arguments.push_back("-clp:NoSummary");
    desc.arguments.push_back("-p:ContinuousIntegrationBuild=true");
    for (const auto &[name, value] : request.properties) {
        desc.arguments.push_back("-p:" + name + "=" + value);
    }
    desc.captureStdout = true;
    desc.captureStderr = true;
    desc.mergeStderr = true;
    desc.timeoutSeconds = request.timeoutSeconds;
    // dotnet's first-run telemetry banner and MSBuild node reuse only add noise and lingering processes.
    desc.environment = {{"DOTNET_CLI_TELEMETRY_OPTOUT", "1"}, {"DOTNET_NOLOGO", "1"}, {"MSBUILDNODEREUSE", "0"}};
    return desc;
}

build_report managed_build_runner::run_blocking(const build_request &request, std::string_view dotnet) {
    build_report report = {};
    core::result<core::process_result> run = core::process::run(command_line(request, dotnet));
    if (!run.has_value()) {
        report.launch = run.get_error();
        report.succeeded = false;
        CONJURE_LOG_WARN(LOG_SCRIPTING, "dotnet build could not start: {}", run.get_error());
        return report;
    }
    const core::process_result &result = run.value();
    report.exitCode = result.exitCode;
    report.timedOut = result.timedOut;
    report.output = result.standardOutput;
    report.elapsedSeconds = result.elapsedSeconds;
    report.diagnostics = parse_output(report.output);
    report.succeeded = result.exitCode == 0 && !result.timedOut;
    if (!report.succeeded) {
        report.launch = core::make_error(
          script_error::BUILD_FAILED,
          "dotnet build of '{}' exited with {} ({} error(s))",
          detail::path_to_utf8(request.projectPath),
          result.exitCode,
          report.error_count()
        );
    }
    return report;
}
