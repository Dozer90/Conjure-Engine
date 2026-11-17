#include "conjure/testing/doctest.h"

#include "conjure/editor/editor_error.h"
#include "conjure/editor/editor_options.h"

#include <array>
#include <initializer_list>
#include <string>
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
        const std::array<std::string_view, 21> arguments = {
            "--project", "C:/proj", "--headless", "--offscreen", "--exec", "run.json", "--out", "out.ndjson",
            "--keep-going", "--ai-port", "5000", "--ai-token", "secret", "--timeout-seconds", "7.5", "--dotnet",
            "--ai-inspect-token", "look", "--trust-project", "--trust-store", "state/trusted.json",
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
        CHECK(options.aiInspectToken == "look");
        CHECK(options.trustProject);
        CHECK(options.trustStoreFile.generic_string() == "state/trusted.json");
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

    TEST_CASE("editor_options - engine flags are forwarded to the engine") {
        const std::array<std::string_view, 5> arguments = {"--frames", "10", "--no-scripting", "--rhi", "null"};
        const core::result<editor_options> parsed = parse(arguments);
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().noScripting);
        REQUIRE(parsed.value().engineArguments.size() == 5);
        CHECK(parsed.value().engineArguments[0] == "--frames");
        CHECK(parsed.value().engineArguments[2] == "--no-scripting");
    }

    TEST_CASE("regression - F083 editor_options stdout_carries_protocol for --ai-stdio, bare --exec, --export") {
        const std::array<std::string_view, 2> stdio = {"--headless", "--ai-stdio"};
        const core::result<editor_options> stdioParsed = parse(stdio);
        CONJURE_REQUIRE_OK(stdioParsed);
        CHECK(stdioParsed.value().stdout_carries_protocol());

        const std::array<std::string_view, 3> exec = {"--headless", "--exec", "f.json"};
        const core::result<editor_options> execParsed = parse(exec);
        CONJURE_REQUIRE_OK(execParsed);
        CHECK(execParsed.value().stdout_carries_protocol());

        const std::array<std::string_view, 4> exported = {"--project", "p", "--export", "out"};
        const core::result<editor_options> exportParsed = parse(exported);
        CONJURE_REQUIRE_OK(exportParsed);
        CHECK(exportParsed.value().stdout_carries_protocol());

        const std::array<std::string_view, 5> toFile = {"--headless", "--exec", "f.json", "--out", "o.ndjson"};
        const core::result<editor_options> toFileParsed = parse(toFile);
        CONJURE_REQUIRE_OK(toFileParsed);
        CHECK_FALSE(toFileParsed.value().stdout_carries_protocol());

        const core::result<editor_options> windowed = parse_editor_options({});
        CONJURE_REQUIRE_OK(windowed);
        CHECK_FALSE(windowed.value().stdout_carries_protocol());

        const std::array<std::string_view, 3> server = {"--headless", "--ai-port", "5000"};
        const core::result<editor_options> serverParsed = parse(server);
        CONJURE_REQUIRE_OK(serverParsed);
        CHECK_FALSE(serverParsed.value().stdout_carries_protocol());
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

    TEST_CASE("regression - F029 editor_options --distribution is parsed, needs a value and needs --export") {
        const std::array<std::string_view, 6> exported = {
            "--project", "p", "--export", "out", "--distribution", "dist/bin",
        };
        const core::result<editor_options> parsed = parse(exported);
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().exportDistribution.generic_string() == "dist/bin");
        CHECK(parsed.value().exportDirectory.generic_string() == "out");
        // The editor's own flag: the engine parser never sees it.
        CHECK(parsed.value().engineArguments.empty());

        const std::array<std::string_view, 5> missingValue = {"--project", "p", "--export", "out", "--distribution"};
        CONJURE_CHECK_ERROR(parse(missingValue), protocol_error_code::INVALID_ARGS);
        const std::array<std::string_view, 4> withoutExport = {"--project", "p", "--distribution", "dist"};
        CONJURE_CHECK_ERROR(parse(withoutExport), protocol_error_code::INVALID_ARGS);
    }

    TEST_CASE("editor_options - --help sets showHelp and the help text names every flag") {
        const std::array<std::string_view, 1> help = {"--help"};
        const core::result<editor_options> parsed = parse(help);
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().showHelp);
        const std::string text = editor_options_help();
        for (const std::string_view flag : {"--project", "--headless", "--offscreen", "--exec", "--out", "--keep-going",
               "--ai-port", "--ai-stdio", "--ai-token", "--ai-inspect-token", "--timeout-seconds", "--export",
               "--distribution", "--trust-project", "--trust-store", "--no-scripting", "--scripting", "--dotnet"}) {
            CHECK_MESSAGE(text.find(flag) != std::string::npos, flag);
        }
    }

    TEST_CASE("editor_options - the help text also lists the engine options the editor hands on") {
        const std::string text = editor_options_help();
        CHECK(text.find("Conjure engine options:") != std::string::npos);
        for (const std::string_view flag : {"--rhi", "--frames", "--log-level", "--fixed-hz", "--jobs", "--scene"}) {
            CHECK_MESSAGE(text.find(flag) != std::string::npos, flag);
        }
        // The flags the editor owns are described once, by the editor, and an unknown option is refused, not passed
        // on as the engine's own text says.
        CHECK(text.find("null window, null device, null audio") == std::string::npos);
        CHECK(text.find("Unknown options are passed") == std::string::npos);
        // The engine's --scripting line says it wins over --headless, which the editor's own rule overrides.
        CHECK(text.find("wins over project.json and --headless") == std::string::npos);
        for (const std::string_view flag : {"--project", "--headless", "--scripting", "--no-scripting", "--help"}) {
            CAPTURE(flag);
            const std::string line = "\n  " + std::string(flag) + " ";
            const usize first = text.find(line);
            REQUIRE(first != std::string::npos);
            CHECK(text.find(line, first + 1) == std::string::npos);
        }
    }

    TEST_CASE("regression - code sweep F200 F790 editor_options the --distribution line names what export copies") {
        const std::string text = editor_options_help();
        for (const std::string_view name : {"THIRD_PARTY_NOTICES.txt", "nethost.dll", "C++ runtime DLLs"}) {
            CHECK_MESSAGE(text.find(name) != std::string::npos, name);
        }
    }

    TEST_CASE("editor_options - --timeout-seconds rejects zero, negative and non-finite values") {
        for (const std::string_view value : {"0", "-1", "inf", "nan", "soon"}) {
            CAPTURE(value);
            const std::array<std::string_view, 2> arguments = {"--timeout-seconds", value};
            CONJURE_CHECK_ERROR(parse(arguments), protocol_error_code::INVALID_ARGS);
        }
        const std::array<std::string_view, 2> half = {"--timeout-seconds", "0.5"};
        const core::result<editor_options> parsed = parse(half);
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().timeoutSeconds == doctest::Approx(0.5));
    }

    TEST_CASE("regression - code sweep F740 editor_options the token is optional, and an empty one is refused") {
        // Without the flag the token is left to the server, which generates one for its TCP transport.
        const core::result<editor_options> defaults = parse_editor_options({});
        CONJURE_REQUIRE_OK(defaults);
        CHECK(defaults.value().aiToken.empty());

        const std::array<std::string_view, 2> empty = {"--ai-token", ""};
        const core::result<editor_options> refused = parse(empty);
        CONJURE_CHECK_ERROR(refused, protocol_error_code::INVALID_ARGS);
        CHECK(refused.get_error().message().find("--ai-token") != std::string::npos);
        const std::array<std::string_view, 1> missing = {"--ai-token"};
        CONJURE_CHECK_ERROR(parse(missing), protocol_error_code::INVALID_ARGS);

        // The usage says where clients read the token and that a command line is no place for a secret.
        const std::string text = editor_options_help();
        CHECK(text.find(".conjure/ai_token") != std::string::npos);
        CHECK(text.find("visible to other users") != std::string::npos);
    }

    TEST_CASE("regression - code sweep F187 editor_options the inspect-only token is optional, must not be empty "
              "and must differ from the full-control token") {
        const core::result<editor_options> defaults = parse_editor_options({});
        CONJURE_REQUIRE_OK(defaults);
        CHECK(defaults.value().aiInspectToken.empty());

        SUBCASE("given alone, it leaves the full-control token to the server") {
            const std::array<std::string_view, 2> alone = {"--ai-inspect-token", "look"};
            const core::result<editor_options> parsed = parse(alone);
            CONJURE_REQUIRE_OK(parsed);
            CHECK(parsed.value().aiInspectToken == "look");
            CHECK(parsed.value().aiToken.empty());
        }
        SUBCASE("an empty value and a missing value are refused") {
            const std::array<std::string_view, 2> empty = {"--ai-inspect-token", ""};
            const core::result<editor_options> refused = parse(empty);
            CONJURE_CHECK_ERROR(refused, protocol_error_code::INVALID_ARGS);
            CHECK(refused.get_error().message().find("--ai-inspect-token") != std::string::npos);
            const std::array<std::string_view, 1> missing = {"--ai-inspect-token"};
            CONJURE_CHECK_ERROR(parse(missing), protocol_error_code::INVALID_ARGS);
        }
        SUBCASE("the same text for both tokens is refused, in either order, and the message shows neither") {
            const std::array<std::string_view, 4> fullFirst = {
                "--ai-token", "same-secret", "--ai-inspect-token", "same-secret",
            };
            const std::array<std::string_view, 4> inspectFirst = {
                "--ai-inspect-token", "same-secret", "--ai-token", "same-secret",
            };
            for (const std::array<std::string_view, 4> &arguments : {fullFirst, inspectFirst}) {
                CAPTURE(arguments[0]);
                const core::result<editor_options> refused = parse(arguments);
                CONJURE_CHECK_ERROR(refused, protocol_error_code::INVALID_ARGS);
                CHECK(refused.get_error().message().find("must differ") != std::string::npos);
                CHECK(refused.get_error().message().find("same-secret") == std::string::npos);
            }
        }
        SUBCASE("two different tokens are both kept") {
            const std::array<std::string_view, 4> both = {"--ai-token", "secret", "--ai-inspect-token", "look"};
            const core::result<editor_options> parsed = parse(both);
            CONJURE_REQUIRE_OK(parsed);
            CHECK(parsed.value().aiToken == "secret");
            CHECK(parsed.value().aiInspectToken == "look");
        }
        SUBCASE("the usage names the file of each token") {
            const std::string text = editor_options_help();
            CHECK(text.find("ai_token_inspect") != std::string::npos);
            CHECK(text.find("--ai-inspect-token <t>") != std::string::npos);
        }
    }

    TEST_CASE("regression - code sweep F187 editor_options a project is trusted only by --trust-project, which "
              "needs --project, and --trust-store names a file") {
        // Nothing is trusted and nothing is remembered unless the command line says so.
        const std::array<std::string_view, 2> plain = {"--project", "p"};
        const core::result<editor_options> defaults = parse(plain);
        CONJURE_REQUIRE_OK(defaults);
        CHECK_FALSE(defaults.value().trustProject);
        CHECK(defaults.value().trustStoreFile.empty());

        SUBCASE("--trust-project answers for the project of --project, wherever the two stand") {
            const std::array<std::string_view, 3> before = {"--trust-project", "--project", "p"};
            const std::array<std::string_view, 3> after = {"--project", "p", "--trust-project"};
            for (const std::array<std::string_view, 3> &arguments : {before, after}) {
                CAPTURE(arguments[0]);
                const core::result<editor_options> parsed = parse(arguments);
                CONJURE_REQUIRE_OK(parsed);
                CHECK(parsed.value().trustProject);
                CHECK(parsed.value().projectPath.generic_string() == "p");
                // The editor's own flag: the engine parser never sees it.
                CHECK(parsed.value().engineArguments.empty());
            }
        }
        SUBCASE("--trust-project without a project is refused: it is an answer about one project, not a mode") {
            const std::array<std::string_view, 2> alone = {"--headless", "--trust-project"};
            const core::result<editor_options> refused = parse(alone);
            CONJURE_CHECK_ERROR(refused, protocol_error_code::INVALID_ARGS);
            CHECK(refused.get_error().message().find("--trust-project needs --project") != std::string::npos);
        }
        SUBCASE("--trust-store takes a file name, needs one, and trusts nothing by itself") {
            const std::array<std::string_view, 4> named = {"--project", "p", "--trust-store", "state/trusted.json"};
            const core::result<editor_options> parsed = parse(named);
            CONJURE_REQUIRE_OK(parsed);
            CHECK(parsed.value().trustStoreFile.generic_string() == "state/trusted.json");
            CHECK_FALSE(parsed.value().trustProject);
            CHECK(parsed.value().engineArguments.empty());
            const std::array<std::string_view, 2> withoutProject = {"--trust-store", "trusted.json"};
            CONJURE_CHECK_OK(parse(withoutProject));
            const std::array<std::string_view, 2> empty = {"--trust-store", ""};
            const core::result<editor_options> refused = parse(empty);
            CONJURE_CHECK_ERROR(refused, protocol_error_code::INVALID_ARGS);
            CHECK(refused.get_error().message().find("--trust-store") != std::string::npos);
            const std::array<std::string_view, 1> missing = {"--trust-store"};
            CONJURE_CHECK_ERROR(parse(missing), protocol_error_code::INVALID_ARGS);
        }
        SUBCASE("the usage says what the flag permits and that it is not remembered") {
            const std::string text = editor_options_help();
            CHECK(text.find("--trust-project ") != std::string::npos);
            CHECK(text.find("--trust-store <file>") != std::string::npos);
            CHECK(text.find("scripts and native modules") != std::string::npos);
            CHECK(text.find("remembered nowhere") != std::string::npos);
            CHECK(text.find("project.trust") != std::string::npos);
        }
    }
}
