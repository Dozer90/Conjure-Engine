#include "editor_test_support.h"

#include "conjure/testing/test_environment.h"

#include "conjure/core/hash.h"
#include "conjure/core/image.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/process.h"
#include "conjure/core/string_utils.h"
#include "conjure/editor/command_dispatcher.h"
#include "conjure/editor/event_broadcaster.h"
#include "conjure/editor/headless_runner.h"
#include "conjure/scene/scene_serializer.h"
#include "editor/core/net/socket.h"

#if defined(CONJURE_EDITOR_TEST_HAS_DOTNET) && CONJURE_EDITOR_TEST_HAS_DOTNET
    #include "conjure/scripting/script_host.h"
#endif

#include <algorithm>
#include <format>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

#ifndef CONJURE_EDITOR_TEST_DATA_DIR
    #define CONJURE_EDITOR_TEST_DATA_DIR ""
#endif
#ifndef CONJURE_EDITOR_TEST_HAS_DOTNET
    #define CONJURE_EDITOR_TEST_HAS_DOTNET 0
#endif
#ifndef CONJURE_EDITOR_TEST_MANAGED_DIR
    #define CONJURE_EDITOR_TEST_MANAGED_DIR ""
#endif
#ifndef CONJURE_EDITOR_TEST_SCRIPTS_DIR
    #define CONJURE_EDITOR_TEST_SCRIPTS_DIR ""
#endif

using namespace conjure;

namespace editor_test {

    namespace {
        [[nodiscard]] bool loopback_available() noexcept {
            static const bool available = []() {
                if (!editor::net::initialise().has_value()) {
                    return false;
                }
                u16 bound = 0;
                const core::result<editor::net::socket_handle> listener = editor::net::listen_loopback(0, bound);
                return listener.has_value() && bound != 0;
            }();
            return available;
        }

        /// <summary>
        /// The DOTNET_RUNTIME precondition of this executable: the SDK and the test scripts were built with this
        /// tree and nethost finds a runtime (the probe never starts the CLR).
        /// </summary>
        [[nodiscard]] bool dotnet_runtime_available() noexcept {
            static const bool available = []() noexcept {
#if defined(CONJURE_EDITOR_TEST_HAS_DOTNET) && CONJURE_EDITOR_TEST_HAS_DOTNET
                std::error_code probe = {};
                const bool sdkPresent =
                  std::filesystem::is_regular_file(managed_directory() / "Conjure.Managed.dll", probe);
                const bool scriptsPresent = std::filesystem::is_regular_file(test_scripts_assembly(), probe);
                return sdkPresent && scriptsPresent && scripting::is_runtime_available();
#else
                return false;
#endif
            }();
            return available;
        }

        struct probe_installer {
            probe_installer() {
                testing::set_precondition_probe(testing::precondition::LOOPBACK_TCP, &loopback_available);
                testing::set_precondition_probe(testing::precondition::DOTNET_RUNTIME, &dotnet_runtime_available);
            }
        };

        const probe_installer sProbeInstaller = {};

        void write_project_file(const std::filesystem::path &directory, const headless_editor_options &options) {
            std::error_code ignored = {};
            std::filesystem::create_directories(directory / "assets" / "scenes", ignored);
            std::filesystem::create_directories(directory / "assets" / "prefabs", ignored);
            std::filesystem::create_directories(directory / "scripts", ignored);
            std::string text = std::format(
              "{{\n  \"schema_version\": 1,\n  \"name\": \"{}\",\n  \"assets_root\": \"assets\"",
              options.projectName
            );
            if (!options.extraProjectJson.empty()) {
                text += ",\n  ";
                text += options.extraProjectJson;
            }
            text += "\n}\n";
            std::ofstream stream(directory / "project.json", std::ios::binary | std::ios::trunc);
            REQUIRE(stream.is_open());
            stream << text;
        }
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // headless_editor

    headless_editor::headless_editor(const headless_editor_options &options) {
        editor::editor_options editorOptions = {};
        editorOptions.headless = true;
        editorOptions.aiPort = u16{0};
        editorOptions.noScripting = !options.scripting;
        editorOptions.dotnetHost = options.dotnetHost;
        editorOptions.timeoutSeconds = 30.0;
        editorOptions.engineArguments = options.engineArguments;
        editorOptions.engineArguments.emplace_back("--log-level");
        editorOptions.engineArguments.emplace_back("warn");
        if (options.withProject) {
            write_project_file(mDirectory.path(), options);
            editorOptions.projectPath = mDirectory.path();
        }
        const core::result<void> initialised = mApplication.initialise(editorOptions, {});
        REQUIRE_MESSAGE(initialised.has_value(), testing::describe_error(initialised));
        mInitialised = true;
        mClientId = context().events().register_client([this](const editor::protocol_event &event) {
            mEvents.push_back(event);
        });
        context().register_response_sink(mClientId, [this](const editor::protocol_response &response) {
            mCaptured = response;
        });
        context().set_dotnet_executable(missing_dotnet_executable());
        if (options.withProject && options.withScene) {
            (void) ok("scene.new", core::json::object({{"name", "test_scene"}, {"path", "scenes/test.scene.json"}}));
            clear_events();
        }
    }

    headless_editor::~headless_editor() {
        if (mInitialised) {
            context().unregister_response_sink(mClientId);
            context().events().unregister_client(mClientId);
        }
        mApplication.shutdown();
    }

    editor::protocol_response headless_editor::send(std::string_view command, core::json::document args) {
        editor::protocol_request request = {};
        request.id = editor::make_id(mNextId++);
        request.command = std::string(command);
        request.args = std::move(args);
        request.clientId = mClientId;
        mCaptured.reset();
        std::optional<editor::protocol_response> response = context().dispatcher().dispatch(request);
        if (response.has_value()) {
            return *response;
        }
        // An asynchronous reply arrives through the sink some frames later. Frames are pumped until it does.
        // The application clock is wall time (the headless forced 1/60 s delta shapes each frame's delta, not the
        // clock), and the dispatcher's own timeout runs on that clock and answers every pending request with
        // TIMEOUT, so a reply always arrives in a well-behaved editor; the loop is a fallback for a dispatcher
        // that never answers and gives up after twice the dispatcher's timeout. There is no frame cap: a request
        // that waits on a real process (script.compile runs dotnet) needs real seconds, and a Development build
        // pumps an allocation-free headless frame in microseconds, so 100,000 frames passed before dotnet could
        // answer (performance round).
        const f64 pumpStart = mApplication.engine().clock().now_seconds();
        const f64 pumpLimit = mApplication.options().timeoutSeconds * 2.0 + 5.0;
        while (!mCaptured.has_value()) {
            const core::result<void> stepped = mApplication.pump_frame();
            REQUIRE_MESSAGE(stepped.has_value(), testing::describe_error(stepped));
            const f64 elapsed = mApplication.engine().clock().now_seconds() - pumpStart;
            if (elapsed >= pumpLimit) {
                break;
            }
        }
        REQUIRE_MESSAGE(mCaptured.has_value(), std::format("'{}' never answered", command));
        editor::protocol_response captured = *mCaptured;
        mCaptured.reset();
        return captured;
    }

    core::json::document headless_editor::ok(std::string_view command, core::json::document args) {
        const editor::protocol_response response = send(command, std::move(args));
        REQUIRE_MESSAGE(
          response.ok,
          std::format("'{}' failed: {} {}", command, editor::to_string(response.errorCode), response.errorMessage)
        );
        return response.result;
    }

    editor::protocol_error_code headless_editor::fail(std::string_view command, core::json::document args) {
        const editor::protocol_response response = send(command, std::move(args));
        REQUIRE_MESSAGE(!response.ok, std::format("'{}' succeeded but was expected to fail", command));
        return response.errorCode;
    }

    void headless_editor::pump(u32 frames) {
        for (u32 frame = 0; frame < frames; ++frame) {
            const core::result<void> stepped = mApplication.pump_frame();
            REQUIRE_MESSAGE(stepped.has_value(), testing::describe_error(stepped));
        }
    }

    bool headless_editor::saw_event(std::string_view name) const {
        return std::any_of(mEvents.begin(), mEvents.end(), [name](const editor::protocol_event &event) {
            return event.name == name;
        });
    }

    void headless_editor::clear_events() {
        mEvents.clear();
    }

    u64 headless_editor::world_hash() {
        return core::hash::fnv1a64(std::string_view(serialised_scene()));
    }

    std::string headless_editor::serialised_scene() {
        core::json::writer out = {};
        const core::result<void> saved = scene::scene_serializer::save_scene(scene(), out);
        REQUIRE_MESSAGE(saved.has_value(), testing::describe_error(saved));
        return out.finish();
    }

    std::string headless_editor::create_entity(std::string_view name, std::string_view parentGuid) {
        core::json::document args = core::json::object({{"name", name}});
        if (!parentGuid.empty()) {
            args.set("parent", parentGuid);
        }
        const core::json::document result = ok("entity.create", std::move(args));
        return std::string(result.root()["guid"].as_string());
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // Snapshots

    std::map<std::string, std::string> project_file_snapshot(const std::filesystem::path &projectRoot) {
        std::map<std::string, std::string> files = {};
        std::error_code ignored = {};
        for (const char *folder : {"assets", "scripts"}) {
            const std::filesystem::path root = projectRoot / folder;
            if (!std::filesystem::is_directory(root, ignored)) {
                continue;
            }
            for (const std::filesystem::directory_entry &entry : std::filesystem::recursive_directory_iterator(
              root,
              ignored
            )) {
                if (!entry.is_regular_file(ignored)) {
                    continue;
                }
                std::ifstream input(entry.path(), std::ios::binary);
                std::stringstream buffer = {};
                buffer << input.rdbuf();
                files[entry.path().lexically_relative(projectRoot).generic_string()] = buffer.str();
            }
        }
        return files;
    }

    editor_snapshot capture_snapshot(headless_editor &editor) {
        editor_snapshot snapshot = {};
        snapshot.worldHash = editor.world_hash();
        snapshot.scene = editor.serialised_scene();
        snapshot.files = project_file_snapshot(editor.project_directory());
        return snapshot;
    }

    std::string describe_difference(const editor_snapshot &before, const editor_snapshot &after) {
        if (before.scene != after.scene) {
            usize index = 0;
            while (
              index < before.scene.size() && index < after.scene.size() && before.scene[index] == after.scene[index]
            ) {
                ++index;
            }
            const usize start = index > 40 ? index - 40 : 0;
            return std::format(
              "scene text differs at byte {}: '{}' vs '{}'",
              index,
              before.scene.substr(start, 160),
              after.scene.substr(start, 160)
            );
        }
        if (before.worldHash != after.worldHash) {
            return std::format("world hash {:016x} -> {:016x}", before.worldHash, after.worldHash);
        }
        for (const auto &[path, content] : before.files) {
            const auto found = after.files.find(path);
            if (found == after.files.end()) {
                return std::format("file '{}' disappeared", path);
            }
            if (found->second != content) {
                return std::format("file '{}' changed ({} -> {} bytes)", path, content.size(), found->second.size());
            }
        }
        for (const auto &[path, content] : after.files) {
            if (!before.files.contains(path)) {
                return std::format("file '{}' appeared", path);
            }
        }
        return "no difference";
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // Fixtures

    std::filesystem::path fixture_directory() {
        return std::filesystem::path(CONJURE_EDITOR_TEST_DATA_DIR) / "commands";
    }

    std::vector<command_fixture> load_command_fixtures() {
        std::vector<command_fixture> fixtures = {};
        std::error_code ignored = {};
        const std::filesystem::path directory = fixture_directory();
        REQUIRE_MESSAGE(std::filesystem::is_directory(directory, ignored), directory.generic_string());
        for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(directory, ignored)) {
            if (!entry.is_regular_file(ignored) || entry.path().extension() != ".json") {
                continue;
            }
            command_fixture fixture = {};
            fixture.name = entry.path().stem().generic_string();
            fixture.path = entry.path();
            core::result<core::json::document> parsed = core::json::document::parse(read_text_file(entry.path()));
            REQUIRE_MESSAGE(parsed.has_value(), std::format("{}: {}", fixture.name, testing::describe_error(parsed)));
            if (!parsed.value().root().is_object()) {
                // Command files (JSON arrays such as build_grid_sweeper.json) share the directory.
                continue;
            }
            fixture.document = std::move(parsed).value();
            fixtures.push_back(std::move(fixture));
        }
        std::sort(fixtures.begin(), fixtures.end(), [](const command_fixture &left, const command_fixture &right) {
            return left.name < right.name;
        });
        return fixtures;
    }

    bool json_contains(const core::json::value &actual, const core::json::value &expected) {
        if (expected.is_object()) {
            if (!actual.is_object()) {
                return false;
            }
            for (const core::json::member entry : expected.members()) {
                if (!actual.contains(entry.key)) {
                    return false;
                }
                if (!json_contains(actual[entry.key], entry.item)) {
                    return false;
                }
            }
            return true;
        }
        if (expected.is_array()) {
            if (!actual.is_array() || actual.size() != expected.size()) {
                return false;
            }
            for (usize index = 0; index < expected.size(); ++index) {
                if (!json_contains(actual[index], expected[index])) {
                    return false;
                }
            }
            return true;
        }
        if (expected.is_number() && actual.is_number()) {
            return std::abs(expected.as_f64() - actual.as_f64()) <= 1.0e-5 * std::max(1.0, std::abs(expected.as_f64()));
        }
        return actual == expected;
    }

    std::string read_text_file(const std::filesystem::path &path) {
        std::ifstream input(path, std::ios::binary);
        REQUIRE_MESSAGE(input.is_open(), path.generic_string());
        std::stringstream buffer = {};
        buffer << input.rdbuf();
        return buffer.str();
    }

    std::string expand_project(std::string_view text, const std::filesystem::path &projectRoot) {
        return core::str::replace_all(text, editor::headless_runner::PROJECT_PLACEHOLDER, projectRoot.generic_string());
    }

    bool dotnet_available() {
        static const bool available = []() {
            core::process_desc desc = {};
            desc.executable = "dotnet";
            desc.arguments = {"--version"};
            desc.timeoutSeconds = 30.0;
            const core::result<core::process_result> run = core::process::run(desc);
            return run.has_value() && run.value().exitCode == 0 && !run.value().timedOut;
        }();
        return available;
    }

    std::string missing_dotnet_executable() {
        return "conjure-editor-tests-no-dotnet-here";
    }

    std::filesystem::path managed_directory() {
        return std::filesystem::path(CONJURE_EDITOR_TEST_MANAGED_DIR);
    }

    std::filesystem::path test_scripts_assembly() {
        const std::filesystem::path root(CONJURE_EDITOR_TEST_SCRIPTS_DIR);
        if (root.empty()) {
            return {};
        }
        return root / "v1" / "Conjure.TestScripts.dll";
    }

    std::vector<std::byte> tiny_png_bytes() {
        core::image image(2, 2);
        image.set_pixel(0, 0, core::rgba8{255, 0, 0, 255});
        image.set_pixel(1, 0, core::rgba8{0, 255, 0, 255});
        image.set_pixel(0, 1, core::rgba8{0, 0, 255, 255});
        image.set_pixel(1, 1, core::rgba8{255, 255, 255, 255});
        core::result<std::vector<std::byte>> encoded = image.encode_png();
        REQUIRE_MESSAGE(encoded.has_value(), testing::describe_error(encoded));
        return std::move(encoded).value();
    }

    std::string tiny_png_base64() {
        static constexpr std::string_view ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        const std::vector<std::byte> bytes = tiny_png_bytes();
        std::string encoded = {};
        usize index = 0;
        while (index < bytes.size()) {
            u32 triple = static_cast<u32>(bytes[index]) << 16;
            const usize rest = bytes.size() - index;
            if (rest > 1) {
                triple |= static_cast<u32>(bytes[index + 1]) << 8;
            }
            if (rest > 2) {
                triple |= static_cast<u32>(bytes[index + 2]);
            }
            encoded.push_back(ALPHABET[(triple >> 18) & 63]);
            encoded.push_back(ALPHABET[(triple >> 12) & 63]);
            encoded.push_back(rest > 1 ? ALPHABET[(triple >> 6) & 63] : '=');
            encoded.push_back(rest > 2 ? ALPHABET[triple & 63] : '=');
            index += 3;
        }
        return encoded;
    }

    std::filesystem::path write_tiny_png(const std::filesystem::path &path) {
        std::error_code ignored = {};
        std::filesystem::create_directories(path.parent_path(), ignored);
        const std::vector<std::byte> bytes = tiny_png_bytes();
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        REQUIRE_MESSAGE(output.is_open(), path.generic_string());
        output.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        return path;
    }
}
