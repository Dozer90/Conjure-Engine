#include "conjure/testing/doctest.h"

#include "conjure/editor/editor_error.h"
#include "conjure/editor/editor_options.h"

#include <array>
#include <string_view>

// editor_options.h: the conjure_editor command line (docs/design/04_editor.md 8).

using namespace conjure;
using namespace conjure::editor;

namespace {
    template <usize Count>
    core::result<editor_options> parse(const std::array<std::string_view, Count> &arguments) {
        return parse_editor_options(std::span<const std::string_view>(arguments));
    }
}

TEST_SUITE("editor/editor_options") {

    TEST_CASE("editor_options - defaults open a window with the TCP server") {
        const core::result<editor_options> parsed = parse_editor_options({});
        CONJURE_REQUIRE_OK(parsed);
        const editor_options &options = parsed.value();
        CHECK_FALSE(options.headless);
        CHECK_FALSE(options.port_is_explicit());
        CHECK(options.wants_tcp_server());
        CHECK(options.first_port() == DEFAULT_AI_PORT);
        CHECK(options.timeoutSeconds == doctest::Approx(120.0));
        CHECK(options.engineArguments.empty());
    }

    TEST_CASE("editor_options - every flag is parsed") {
        const std::array<std::string_view, 16> arguments = {
            "--project", "C:/proj", "--headless", "--offscreen", "--exec", "run.json", "--out", "out.ndjson",
            "--keep-going", "--ai-port", "5000", "--ai-token", "secret", "--timeout-seconds", "7.5", "--dotnet",
        };
        const core::result<editor_options> parsed = parse(arguments);
        CONJURE_REQUIRE_OK(parsed);
        const editor_options &options = parsed.value();
        CHECK(options.projectPath.generic_string() == "C:/proj");
        CHECK(options.headless);
        CHECK(options.offscreen);
        CHECK(options.dotnetHost);
        // --dotnet is the editor's own flag: the engine parser never sees it.
        CHECK(options.engineArguments.empty());
        CHECK(options.execFile.generic_string() == "run.json");
        CHECK(options.outputFile.generic_string() == "out.ndjson");
        CHECK(options.keepGoing);
        CHECK(options.port_is_explicit());
        CHECK(options.first_port() == 5000);
        CHECK(options.aiToken == "secret");
        CHECK(options.timeoutSeconds == doctest::Approx(7.5));
        // An explicit port wins over the --exec default of no server.
        CHECK(options.wants_tcp_server());
    }

    TEST_CASE("editor_options - --exec without a port starts no server, --ai-port 0 disables it") {
        const std::array<std::string_view, 3> exec = {"--headless", "--exec", "a.json"};
        const core::result<editor_options> execParsed = parse(exec);
        CONJURE_REQUIRE_OK(execParsed);
        CHECK_FALSE(execParsed.value().wants_tcp_server());

        const std::array<std::string_view, 2> disabled = {"--ai-port", "0"};
        const core::result<editor_options> disabledParsed = parse(disabled);
        CONJURE_REQUIRE_OK(disabledParsed);
        CHECK_FALSE(disabledParsed.value().wants_tcp_server());

        const std::array<std::string_view, 2> stdio = {"--headless", "--ai-stdio"};
        const core::result<editor_options> stdioParsed = parse(stdio);
        CONJURE_REQUIRE_OK(stdioParsed);
        CHECK(stdioParsed.value().aiStdio);
        CHECK_FALSE(stdioParsed.value().wants_tcp_server());
    }

    TEST_CASE("editor_options - unknown tokens are forwarded to the engine") {
        const std::array<std::string_view, 5> arguments = {"--frames", "10", "--no-scripting", "--rhi", "null"};
        const core::result<editor_options> parsed = parse(arguments);
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().noScripting);
        REQUIRE(parsed.value().engineArguments.size() == 5);
        CHECK(parsed.value().engineArguments[0] == "--frames");
        CHECK(parsed.value().engineArguments[2] == "--no-scripting");
    }

    TEST_CASE("editor_options - inconsistent combinations are INVALID_ARGS") {
        const std::array<std::string_view, 4> execStdio = {"--exec", "a.json", "--ai-stdio", "--headless"};
        CONJURE_CHECK_ERROR(parse(execStdio), protocol_error_code::INVALID_ARGS);
        const std::array<std::string_view, 2> outAlone = {"--out", "x.ndjson"};
        CONJURE_CHECK_ERROR(parse(outAlone), protocol_error_code::INVALID_ARGS);
        const std::array<std::string_view, 1> offscreenAlone = {"--offscreen"};
        CONJURE_CHECK_ERROR(parse(offscreenAlone), protocol_error_code::INVALID_ARGS);
        const std::array<std::string_view, 2> exportAlone = {"--export", "out"};
        CONJURE_CHECK_ERROR(parse(exportAlone), protocol_error_code::INVALID_ARGS);
        const std::array<std::string_view, 2> badPort = {"--ai-port", "lots"};
        CONJURE_CHECK_ERROR(parse(badPort), protocol_error_code::INVALID_ARGS);
        const std::array<std::string_view, 1> missingValue = {"--project"};
        CONJURE_CHECK_ERROR(parse(missingValue), protocol_error_code::INVALID_ARGS);
    }

    TEST_CASE("editor_options - --help sets showHelp and the help text names every flag") {
        const std::array<std::string_view, 1> help = {"--help"};
        const core::result<editor_options> parsed = parse(help);
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().showHelp);
        const std::string_view text = editor_options_help();
        for (const std::string_view flag : {"--project", "--headless", "--offscreen", "--exec", "--out", "--keep-going",
               "--ai-port", "--ai-stdio", "--ai-token", "--timeout-seconds", "--export", "--no-scripting",
               "--dotnet"}) {
            CHECK_MESSAGE(text.find(flag) != std::string_view::npos, flag);
        }
    }
}
