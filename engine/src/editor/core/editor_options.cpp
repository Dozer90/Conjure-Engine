#include "conjure/editor/editor_options.h"

#include "conjure/editor/editor_error.h"

#include "conjure/core/string_utils.h"

#include <cmath>
#include <format>
#include <limits>

namespace conjure::editor {

    namespace {
        constexpr std::string_view HELP_TEXT =
          "conjure_editor [options] [engine options]\n"
          "\n"
          "  --project <dir>          open the project in <dir> (project.json)\n"
          "  --headless               no window: null platform, rhi and audio; scripting on the null host\n"
          "  --offscreen              with --headless: ask for a real rhi device without a swapchain\n"
          "  --exec <file.json>       run the requests of the file (JSON array or NDJSON) and exit\n"
          "  --out <file.ndjson>      write the responses and events of --exec to a file (default stdout)\n"
          "  --keep-going             continue --exec after a failed command\n"
          "  --ai-port <n>            TCP port of the AI control protocol (0 disables; default 47820 + fallback)\n"
          "  --ai-stdio               speak the protocol over stdin/stdout (logging goes to stderr)\n"
          "  --ai-token <t>           clients must present the token in session.hello\n"
          "  --timeout-seconds <s>    asynchronous replies time out after <s> seconds (default 120)\n"
          "  --export <dir>           export the project to <dir> and exit\n"
          "  --no-scripting           run without the scripting runtime\n"
          "  --dotnet                 with --headless: the real .NET script host instead of the null host\n"
          "  --help                   print this text\n"
          "\n"
          "Every other flag is handed to the engine (--rhi, --log-level, --frames, --fixed-hz, ...).\n"
          "Exit codes: 0 ok, 1 a command failed, 2 startup or project failure, 3 bad arguments.\n";

        [[nodiscard]] core::error bad_argument(std::string message) {
            return make_editor_error(protocol_error_code::INVALID_ARGS, std::move(message));
        }

        [[nodiscard]] core::result<std::string_view> take_value(
          std::span<const std::string_view> args,
          usize &index,
          std::string_view flag
        ) {
            if (index + 1 >= args.size()) {
                return bad_argument(std::format("{} needs a value", flag));
            }
            ++index;
            return args[index];
        }
    }

    bool editor_options::wants_tcp_server() const noexcept {
        if (aiPort.has_value()) {
            return *aiPort != 0;
        }
        if (!execFile.empty() || !exportDirectory.empty()) {
            return false;
        }
        return !headless || !aiStdio;
    }

    u16 editor_options::first_port() const noexcept {
        if (aiPort.has_value()) {
            return *aiPort;
        }
        return DEFAULT_AI_PORT;
    }

    core::result<editor_options> parse_editor_options(std::span<const std::string_view> args) {
        editor_options options = {};
        for (usize index = 0; index < args.size(); ++index) {
            const std::string_view token = args[index];
            if (token == "--project") {
                CONJURE_TRY_ASSIGN(const std::string_view value, take_value(args, index, token));
                options.projectPath = std::filesystem::path(value);
            } else if (token == "--headless") {
                options.headless = true;
            } else if (token == "--offscreen") {
                options.offscreen = true;
            } else if (token == "--exec") {
                CONJURE_TRY_ASSIGN(const std::string_view value, take_value(args, index, token));
                options.execFile = std::filesystem::path(value);
            } else if (token == "--out") {
                CONJURE_TRY_ASSIGN(const std::string_view value, take_value(args, index, token));
                options.outputFile = std::filesystem::path(value);
            } else if (token == "--keep-going") {
                options.keepGoing = true;
            } else if (token == "--ai-port") {
                CONJURE_TRY_ASSIGN(const std::string_view value, take_value(args, index, token));
                const std::optional<u64> port = core::str::parse_uint(value);
                if (!port.has_value() || *port > std::numeric_limits<u16>::max()) {
                    return bad_argument(std::format("--ai-port: '{}' is not a port number", value));
                }
                options.aiPort = static_cast<u16>(*port);
            } else if (token == "--ai-stdio") {
                options.aiStdio = true;
            } else if (token == "--ai-token") {
                CONJURE_TRY_ASSIGN(const std::string_view value, take_value(args, index, token));
                options.aiToken = std::string(value);
            } else if (token == "--timeout-seconds") {
                CONJURE_TRY_ASSIGN(const std::string_view value, take_value(args, index, token));
                const std::optional<f64> seconds = core::str::parse_double(value);
                if (!seconds.has_value() || !std::isfinite(*seconds) || *seconds < 0.0) {
                    return bad_argument(std::format("--timeout-seconds: '{}' is not a non-negative number", value));
                }
                options.timeoutSeconds = *seconds;
            } else if (token == "--export") {
                CONJURE_TRY_ASSIGN(const std::string_view value, take_value(args, index, token));
                options.exportDirectory = std::filesystem::path(value);
            } else if (token == "--no-scripting") {
                options.noScripting = true;
                options.engineArguments.emplace_back(token);
            } else if (token == "--dotnet") {
                options.dotnetHost = true;
            } else if (token == "--help" || token == "-h") {
                options.showHelp = true;
            } else {
                options.engineArguments.emplace_back(token);
            }
        }

        if (!options.execFile.empty() && options.aiStdio) {
            return bad_argument("--exec and --ai-stdio cannot be combined: both own stdout");
        }
        if (!options.outputFile.empty() && options.execFile.empty()) {
            return bad_argument("--out needs --exec");
        }
        if (options.offscreen && !options.headless) {
            return bad_argument("--offscreen needs --headless");
        }
        if (!options.exportDirectory.empty() && options.projectPath.empty()) {
            return bad_argument("--export needs --project");
        }
        return options;
    }

    std::string_view editor_options_help() noexcept {
        return HELP_TEXT;
    }
}
