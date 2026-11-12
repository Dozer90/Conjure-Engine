#pragma once

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The conjure_editor command line (docs/design/04_editor.md 3.11, 8): the editor's own flags are parsed here,
// everything else (--rhi, --log-level, --frames, ...) is passed on to the engine's parser through engineArguments.
// A bad editor flag is INVALID_ARGS; an option the engine does not know either, or a bad engine flag value, is
// refused by editor_application::make_engine_desc with BAD_ARGUMENTS. main() exits with 3 for both.

namespace conjure::editor {

    /// <summary>
    /// The default TCP port of the AI control protocol; nine more are tried when it is busy and the port
    /// was not given explicitly.
    /// </summary>
    inline constexpr u16 DEFAULT_AI_PORT = 47820;

    /// <summary>
    /// Ports tried after a busy default port.
    /// </summary>
    inline constexpr u16 AI_PORT_FALLBACK_COUNT = 9;

    struct CONJURE_API editor_options {
        std::filesystem::path projectPath = {};             // --project <dir>
        std::filesystem::path execFile = {};                // --exec <file.json>
        std::filesystem::path outputFile = {};              // --out <responses.ndjson>; empty = stdout
        std::filesystem::path exportDirectory = {};         // --export <dir>
        std::filesystem::path exportDistribution = {};      // --distribution <dir>, with --export
        std::string aiToken = {};                           // --ai-token <t>
        std::vector<std::string> engineArguments = {};      // everything else, for the engine's parser
        std::optional<u16> aiPort = std::nullopt;           // --ai-port <n>; 0 disables the server
        f64 timeoutSeconds = 120.0;                         // --timeout-seconds <s>, asynchronous replies
        bool headless = false;                              // --headless
        bool offscreen = false;                             // --offscreen
        bool keepGoing = false;                             // --keep-going
        bool aiStdio = false;                               // --ai-stdio
        bool noScripting = false;                           // --no-scripting (also passed to the engine)
        bool dotnetHost = false;                            // --dotnet: headless keeps the real .NET script host
        bool showHelp = false;                              // --help

        /// <summary>
        /// True when a TCP server should start: explicit non-zero --ai-port, or no --ai-port in a
        /// windowed session. A headless --exec run starts no server unless a port was given.
        /// </summary>
        [[nodiscard]] bool wants_tcp_server() const noexcept;

        /// <summary>
        /// True when standard output carries protocol data: the responses and events of --ai-stdio, the responses of
        /// --exec without --out, or the project.export response of --export. Logging must then go to standard
        /// error, never standard output (docs/design/04_editor.md 3.11, 3.12); conjure_editor's main does that.
        /// </summary>
        [[nodiscard]] bool stdout_carries_protocol() const noexcept;

        /// <summary>
        /// The port to bind first: the explicit one or DEFAULT_AI_PORT.
        /// </summary>
        [[nodiscard]] u16 first_port() const noexcept;

        /// <summary>
        /// True when the port was given on the command line (no fallback ports are tried).
        /// </summary>
        [[nodiscard]] bool port_is_explicit() const noexcept {
            return aiPort.has_value();
        }

        [[nodiscard]] bool operator==(const editor_options &) const = default;
    };

    /// <summary>
    /// Parses argv[1..]. INVALID_ARGS for a flag without its value, a value that does not parse (port above
    /// 65535, a timeout that is not a positive finite number), --exec together with --ai-stdio, --out without
    /// --exec, or --distribution without --export. Every other token is kept in engineArguments for the engine's
    /// parser, and editor_application::make_engine_desc refuses what that parser does not know either.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<editor_options> parse_editor_options(std::span<const std::string_view> args);

    /// <summary>
    /// The usage text printed for --help and after a bad flag: the editor's own options followed by the engine
    /// options it hands on (engine::command_line_help() without the lines of --project, --headless, --no-scripting
    /// and --help, which the editor's own lines describe, and without the engine's note on unknown options).
    /// </summary>
    [[nodiscard]] CONJURE_API std::string editor_options_help();
}
