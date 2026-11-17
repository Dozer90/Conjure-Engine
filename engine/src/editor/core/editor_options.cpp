#include "conjure/editor/editor_options.h"

#include "conjure/editor/editor_error.h"

#include "conjure/core/string_utils.h"
#include "conjure/engine/command_line.h"

#include <algorithm>
#include <array>
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
          "  --ai-port <n>            TCP port of the AI control protocol (0 disables; default 47820 + fallback);\n"
          "                           a TCP client presents a token in session.hello; the editor generates two\n"
          "                           and writes them next to ai_port: <project>/.conjure/ai_token opens every\n"
          "                           command, ai_token_inspect only the commands that change nothing\n"
          "  --ai-stdio               speak the protocol over stdin/stdout (logging goes to stderr); a headless\n"
          "                           session without --ai-port exits with 0 when stdin ends\n"
          "  --ai-token <t>           use <t> instead of the generated full-control token; clients of every\n"
          "                           transport must then present a token (a command line is\n"
          "                           visible to other users of the machine: prefer the generated tokens there)\n"
          "  --ai-inspect-token <t>   use <t> instead of the generated inspect-only token; clients of every\n"
          "                           transport must then present a token; must differ from --ai-token\n"
          "  --timeout-seconds <s>    asynchronous replies time out after <s> seconds (> 0, default 120)\n"
          "  --export <dir>           export the project to <dir> and exit\n"
          "  --distribution <dir>     with --export: copy from <dir> conjure_runtime, conjure_shared, shaders/,\n"
          "                           THIRD_PARTY_NOTICES.txt and, from an MSVC build, nethost.dll and the Visual\n"
          "                           C++ runtime DLLs (never the debug ones)\n"
          "  --trust-project          trust the project of --project for this run: opening a project runs its\n"
          "                           scripts and native modules with your rights, so one nobody answered for\n"
          "                           is not opened (the window asks; a run without one exits with 2). The\n"
          "                           flag is remembered nowhere; project.trust remembers an answer\n"
          "  --trust-store <file>     keep remembered answers in <file> instead of the per-user trust store\n"
          "  --no-scripting           run without the scripting runtime\n"
          "  --scripting              windowed session: turn scripting on for a project that disables it; no\n"
          "                           effect with --headless; cannot be combined with --no-scripting\n"
          "  --dotnet                 with --headless: the real .NET script host instead of the null host\n"
          "  --help                   print this text\n"
          "\n"
          "Every other flag is handed to the engine (its options follow); an option neither knows is a bad\n"
          "argument. Whenever stdout carries protocol lines (--ai-stdio, --exec without --out, --export) the\n"
          "log goes to stderr.\n"
          "Exit codes: 0 ok, 1 a command failed, 2 startup or project failure, 3 bad arguments,\n"
          "4 graphics device lost.\n";

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

    bool editor_options::stdout_carries_protocol() const noexcept {
        return aiStdio || (!execFile.empty() && outputFile.empty()) || !exportDirectory.empty();
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
                if (value.empty()) {
                    // An empty token would read as "no token asked for" further down and silently become a
                    // generated one; the flag exists to name a token.
                    return bad_argument("--ai-token: the token must not be empty");
                }
                options.aiToken = std::string(value);
            } else if (token == "--ai-inspect-token") {
                CONJURE_TRY_ASSIGN(const std::string_view value, take_value(args, index, token));
                if (value.empty()) {
                    return bad_argument("--ai-inspect-token: the token must not be empty");
                }
                options.aiInspectToken = std::string(value);
            } else if (token == "--timeout-seconds") {
                CONJURE_TRY_ASSIGN(const std::string_view value, take_value(args, index, token));
                const std::optional<f64> seconds = core::str::parse_double(value);
                // Zero would make every asynchronous reply time out the moment it is queued.
                if (!seconds.has_value() || !std::isfinite(*seconds) || *seconds <= 0.0) {
                    return bad_argument(std::format("--timeout-seconds: '{}' is not a positive number", value));
                }
                options.timeoutSeconds = *seconds;
            } else if (token == "--export") {
                CONJURE_TRY_ASSIGN(const std::string_view value, take_value(args, index, token));
                options.exportDirectory = std::filesystem::path(value);
            } else if (token == "--distribution") {
                CONJURE_TRY_ASSIGN(const std::string_view value, take_value(args, index, token));
                options.exportDistribution = std::filesystem::path(value);
            } else if (token == "--trust-project") {
                options.trustProject = true;
            } else if (token == "--trust-store") {
                CONJURE_TRY_ASSIGN(const std::string_view value, take_value(args, index, token));
                if (value.empty()) {
                    // An empty name would read as "no store" further down and silently remember nothing.
                    return bad_argument("--trust-store: the file name must not be empty");
                }
                options.trustStoreFile = std::filesystem::path(value);
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

        if (!options.aiToken.empty() && options.aiToken == options.aiInspectToken) {
            // One text cannot grant two levels: the client an inspect-only token was handed to would hold
            // the full-control one.
            return bad_argument("--ai-token and --ai-inspect-token must differ");
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
        if (options.trustProject && options.projectPath.empty()) {
            // The flag answers for one project, the one named here; it is not a general "yes".
            return bad_argument("--trust-project needs --project");
        }
        if (!options.exportDistribution.empty() && options.exportDirectory.empty()) {
            return bad_argument("--distribution needs --export");
        }
        return options;
    }

    std::string editor_options_help() {
        // The engine's own text lists the flags the editor hands on, so a mistyped one can be checked by eye. The
        // lines of flags the editor owns are dropped (the editor's own line says what they do here), and so is the
        // engine's note on unknown options: the editor refuses those.
        constexpr std::array<std::string_view, 6> DROPPED_PREFIXES = {
            "  --project", "  --headless", "  --scripting", "  --no-scripting", "  --help", "Unknown options",
        };
        std::string text = std::string(HELP_TEXT);
        text += '\n';
        for (const std::string_view line : core::str::split_lines(engine::command_line_help())) {
            const bool dropped = std::ranges::any_of(DROPPED_PREFIXES, [line](std::string_view prefix) {
                return core::str::starts_with(line, prefix);
            });
            if (!dropped) {
                text += line;
                text += '\n';
            }
        }
        return text;
    }
}
