#pragma once

#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"

#include "conjure/core/json/document.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/editor/editor_application.h"
#include "conjure/editor/editor_context.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/editor_options.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/scene/scene.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Shared fixtures of the editor tests (docs/design/04_editor.md 10, 08_testing.md 6.3): a headless editor over a
// scratch project (null platform, rhi and audio, scripting on the null host), request helpers that pump frames
// until an asynchronous reply arrives, state snapshots for the inverse round trips of the command catalogue, the
// fixture file format of tests/editor/data/commands/ and the LOOPBACK_TCP precondition probe.

namespace editor_test {

    struct headless_editor_options {
        bool withProject = true;                        // write and open a scratch project
        bool withScene = true;                          // scene.new after start (needs a project)
        bool scripting = true;                          // the null-host script runtime
        bool dotnetHost = false;                        // --dotnet: the real .NET host (DOTNET_RUNTIME cases)
        std::string projectName = "editor_test";
        std::string extraProjectJson = "";              // extra members of project.json
        std::vector<std::string> engineArguments = {};  // forwarded to the engine
    };

    /// <summary>
    /// A fully initialised editor_application in headless mode with no protocol transport (requests go straight to
    /// the dispatcher as an in-process client) and a temp project. Owns a log_capture: an ERROR fails the case
    /// unless allow_errors() was called.
    /// </summary>
    class headless_editor {
        public:
            static constexpr conjure::u32 MAX_PUMPED_FRAMES = 100000;

        private:
            conjure::testing::log_capture mLog = {};
            conjure::testing::temp_directory mDirectory = {};
            conjure::editor::editor_application mApplication = {};
            std::optional<conjure::editor::protocol_response> mCaptured = std::nullopt;
            std::vector<conjure::editor::protocol_event> mEvents = {};
            conjure::u32 mClientId = 0;
            conjure::i64 mNextId = 1;
            bool mInitialised = false;

        public:
            headless_editor(const headless_editor_options &options = {});
            ~headless_editor();

            headless_editor(const headless_editor &) = delete;
            headless_editor &operator=(const headless_editor &) = delete;

        public:
            [[nodiscard]] bool is_initialised() const noexcept {
                return mInitialised;
            }

            [[nodiscard]] conjure::editor::editor_application &application() noexcept {
                return mApplication;
            }

            [[nodiscard]] conjure::editor::editor_context &context() noexcept {
                return mApplication.context();
            }

            [[nodiscard]] conjure::scene::scene &scene() noexcept {
                return mApplication.context().scene();
            }

            [[nodiscard]] const std::filesystem::path &project_directory() const noexcept {
                return mDirectory.path();
            }

            [[nodiscard]] conjure::testing::log_capture &log() noexcept {
                return mLog;
            }

            /// <summary>
            /// The in-process client id requests are sent as (events are captured for it).
            /// </summary>
            [[nodiscard]] conjure::u32 client_id() const noexcept {
                return mClientId;
            }

            /// <summary>
            /// Dispatches a request and, when it is asynchronous, pumps frames until its response arrives.
            /// </summary>
            [[nodiscard]] conjure::editor::protocol_response send(
              std::string_view command,
              conjure::core::json::document args = conjure::core::json::document::object()
            );

            /// <summary>
            /// send() that REQUIREs success and returns the result.
            /// </summary>
            [[nodiscard]] conjure::core::json::document ok(
              std::string_view command,
              conjure::core::json::document args = conjure::core::json::document::object()
            );

            /// <summary>
            /// send() that REQUIREs failure and returns the error code.
            /// </summary>
            [[nodiscard]] conjure::editor::protocol_error_code fail(
              std::string_view command,
              conjure::core::json::document args = conjure::core::json::document::object()
            );

            /// <summary>
            /// Runs engine frames (the editor ticks inside them).
            /// </summary>
            void pump(conjure::u32 frames = 1);

            /// <summary>
            /// Events emitted to this client since the last clear_events().
            /// </summary>
            [[nodiscard]] const std::vector<conjure::editor::protocol_event> &events() const noexcept {
                return mEvents;
            }

            [[nodiscard]] bool saw_event(std::string_view name) const;
            void clear_events();

            /// <summary>
            /// A content hash of the scene: FNV-1a of serialised_scene(). ecs::world_hash folds entity ids in, which
            /// every restore renumbers, so the editor's "same world" comparisons hash the canonical serialisation.
            /// </summary>
            [[nodiscard]] conjure::u64 world_hash();

            /// <summary>
            /// The scene serialised with scene_serializer::save_scene (canonical text).
            /// </summary>
            [[nodiscard]] std::string serialised_scene();

            /// <summary>
            /// Creates an entity through the protocol and returns its guid.
            /// </summary>
            [[nodiscard]] std::string create_entity(std::string_view name, std::string_view parentGuid = {});
    };

    /// <summary>
    /// Every file under <project>/assets and <project>/scripts as path -> bytes, for the inverse round trips of
    /// asset and script commands.
    /// </summary>
    [[nodiscard]] std::map<std::string, std::string> project_file_snapshot(const std::filesystem::path &projectRoot);

    /// <summary>
    /// A complete editing-state snapshot: world hash, serialised scene and the project files.
    /// </summary>
    struct editor_snapshot {
        conjure::u64 worldHash = 0;
        std::string scene = "";
        std::map<std::string, std::string> files = {};

        [[nodiscard]] bool operator==(const editor_snapshot &) const = default;
    };

    [[nodiscard]] editor_snapshot capture_snapshot(headless_editor &editor);

    /// <summary>
    /// Explains the first difference between two snapshots (for CHECK messages).
    /// </summary>
    [[nodiscard]] std::string describe_difference(const editor_snapshot &before, const editor_snapshot &after);

    /// <summary>
    /// One fixture of tests/editor/data/commands/<name>.json:
    /// { "setup": [{"cmd","args"}...], "request": {"cmd","args"}, "expect": {"ok": bool, "result": {subset},
    ///   "error": {"code"}} , "expectAnyOf": [expect...], "roundTrip": bool, "editor": {"withScene": bool} }
    /// </summary>
    struct command_fixture {
        std::string name = "";
        std::filesystem::path path = {};
        conjure::core::json::document document = {};
    };

    [[nodiscard]] std::vector<command_fixture> load_command_fixtures();

    /// <summary>
    /// The directory of the fixtures (CONJURE_EDITOR_TEST_DATA_DIR/commands).
    /// </summary>
    [[nodiscard]] std::filesystem::path fixture_directory();

    /// <summary>
    /// True when 'actual' contains every member of 'expected' (recursively; arrays element-wise, scalars equal).
    /// </summary>
    [[nodiscard]] bool json_contains(
      const conjure::core::json::value &actual,
      const conjure::core::json::value &expected
    );

    /// <summary>
    /// Reads a whole text file; fails the case when it cannot be read.
    /// </summary>
    [[nodiscard]] std::string read_text_file(const std::filesystem::path &path);

    /// <summary>
    /// Replaces ${project} in a text (the fixtures' placeholder).
    /// </summary>
    [[nodiscard]] std::string expand_project(std::string_view text, const std::filesystem::path &projectRoot);

    /// <summary>
    /// True when a dotnet executable answers --version within a few seconds.
    /// </summary>
    [[nodiscard]] bool dotnet_available();

    /// <summary>
    /// An executable name that never exists, so script.compile fails fast with NOT_AVAILABLE.
    /// </summary>
    [[nodiscard]] std::string missing_dotnet_executable();

    /// <summary>
    /// Directory holding Conjure.Bootstrap.* and Conjure.Managed.dll; empty when the SDK was not built.
    /// </summary>
    [[nodiscard]] std::filesystem::path managed_directory();

    /// <summary>
    /// Conjure.TestScripts.dll of the v1 variant (the probes of the [dotnet] cases); empty when the SDK was not
    /// built.
    /// </summary>
    [[nodiscard]] std::filesystem::path test_scripts_assembly();

    /// <summary>
    /// A tiny valid PNG (2x2 RGBA) for asset.import fixtures.
    /// </summary>
    [[nodiscard]] std::vector<std::byte> tiny_png_bytes();

    /// <summary>
    /// tiny_png_bytes() as standard base64 (the payload of asset.restore).
    /// </summary>
    [[nodiscard]] std::string tiny_png_base64();

    /// <summary>
    /// Writes tiny_png_bytes() to a file and returns the path.
    /// </summary>
    [[nodiscard]] std::filesystem::path write_tiny_png(const std::filesystem::path &path);
}
