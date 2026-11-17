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
#include "conjure/editor/protocol/loopback_transport.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/scene/scene.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Shared fixtures of the editor tests (docs/design/04_editor.md 10, 08_testing.md 6.3): a headless editor over a
// scratch project (null platform, rhi and audio, scripting on the null host), request helpers that pump frames
// until an asynchronous reply arrives, a protocol client of that editor on a loopback transport, state snapshots
// for the inverse round trips of the command catalogue, the
// fixture file format of tests/editor/data/commands/, the LOOPBACK_TCP precondition probe and the two helpers
// that move bytes over a test's loopback socket.

namespace conjure::editor::net {
    class socket_handle;
}

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
    /// the dispatcher as an in-process client) and a temp project, trusted for the session and without a trust
    /// store. Owns a log_capture: an ERROR fails the case unless allow_errors() was called.
    /// </summary>
    class headless_editor {
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
            /// Trusts another project for this editor's session, the way the fixture trusts its own scratch project
            /// (04_editor.md D27): a case that writes a second project by hand calls this before it opens it. A
            /// project made with project.create in a new folder needs no call.
            /// </summary>
            void trust_project(const std::filesystem::path &root);

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

            /// <summary>
            /// The `warning` events carrying the given code among events(), in the order they arrived.
            /// </summary>
            [[nodiscard]] std::vector<conjure::editor::protocol_event> warnings(std::string_view code) const;

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
    /// A protocol client of a headless_editor on a loopback transport of its own: unlike the editor's in-process
    /// client it has a transport entry in the protocol server, so the token handshake, the scope of its token
    /// and the audit log apply to it. Sends a line, pumps frames until the response with the id arrives, and
    /// keeps the events seen on the way. The editor must outlive the session.
    /// </summary>
    class loopback_session {
        private:
            headless_editor *mEditor = nullptr;
            conjure::editor::loopback_client mClient = {};
            std::vector<conjure::editor::protocol_event> mEvents = {};
            conjure::i64 mNextId = 1;

        public:
            explicit loopback_session(headless_editor &editor);

        public:
            /// <summary>
            /// Sends the command with the next id and returns its response.
            /// </summary>
            [[nodiscard]] conjure::editor::protocol_response request(
              std::string_view command,
              conjure::core::json::document args = conjure::core::json::document::object()
            );

            /// <summary>
            /// Sends 'line' as it is and pumps frames until the response that echoes 'expectedId' arrived; fails
            /// the case when none does.
            /// </summary>
            [[nodiscard]] conjure::editor::protocol_response send_line(
              const std::string &line,
              conjure::i64 expectedId
            );

            /// <summary>
            /// request() that REQUIREs success and returns the result.
            /// </summary>
            [[nodiscard]] conjure::core::json::document ok(
              std::string_view command,
              conjure::core::json::document args = conjure::core::json::document::object()
            );

            /// <summary>
            /// Events that reached the client so far.
            /// </summary>
            [[nodiscard]] const std::vector<conjure::editor::protocol_event> &events() const noexcept {
                return mEvents;
            }

            [[nodiscard]] conjure::usize count_events(std::string_view name) const;

            [[nodiscard]] conjure::editor::loopback_client &client() noexcept {
                return mClient;
            }
    };

    /// <summary>
    /// Writes a minimal project by hand, the way a project that came from elsewhere lies on disk: the folder, its
    /// assets folder and a project.json with the name and assets_root; 'extraMembers' are further members of the
    /// root object (R"("startup_scene": "scenes/main.scene.json")"). The editor trusts no such project by itself
    /// (04_editor.md D27): a case opens it with trustProject / --trust-project, headless_editor::trust_project or
    /// the project.trust command. Fails the case when project.json cannot be written.
    /// </summary>
    void write_project(
      const std::filesystem::path &directory,
      std::string_view name,
      std::string_view extraMembers = {}
    );

    /// <summary>
    /// project_trust::project_key of a project root (what the trust store and the answers of the trust commands
    /// name the project by); fails the case when the path has none.
    /// </summary>
    [[nodiscard]] std::string project_key_of(const std::filesystem::path &projectRoot);

    /// <summary>
    /// Whether 'directory' lies on a volume where a file can be restricted to its owner
    /// (core::vfs::write_private_file; the probe leaves an empty file "owner_only_probe" there). False, with a
    /// MESSAGE that says the case did not run, on FAT, exFAT and the like: the trust store, the token files and
    /// the audit log are owner-only files, so a case about one of them returns when this is false.
    /// </summary>
    [[nodiscard]] bool owner_only_files_available(const std::filesystem::path &directory);

    /// <summary>
    /// Makes a file impossible to replace for as long as the object lives: the way a case reaches the failure
    /// path of code that rewrites a file through core::vfs::write_private_file (a temporary beside the file,
    /// renamed over it). Windows: the file is held open by a reader, which forbids renaming over it. POSIX: the
    /// file's folder loses its write permission, so no temporary can be created in it; the folder should hold
    /// nothing else the case writes meanwhile. active() is false, after a MESSAGE that says the case did not
    /// run, where that does not hold the account back: one that passes every permission (root in a container).
    /// The file must exist. The destructor gives the folder its permissions back.
    /// </summary>
    class replace_blocker {
        private:
            std::filesystem::path mFolder = {};
            std::ifstream mReader = {};
            bool mActive = false;

        public:
            explicit replace_blocker(const std::filesystem::path &file);
            ~replace_blocker();

            replace_blocker(const replace_blocker &) = delete;
            replace_blocker &operator=(const replace_blocker &) = delete;

        public:
            [[nodiscard]] bool active() const noexcept {
                return mActive;
            }
    };

    /// <summary>
    /// The asset path of the scene a headless_editor starts with (withScene). The scene has no file until it is
    /// saved.
    /// </summary>
    inline constexpr std::string_view TEST_SCENE = "assets://scenes/test.scene.json";

    /// <summary>
    /// The file TEST_SCENE names in the editor's project.
    /// </summary>
    [[nodiscard]] std::filesystem::path test_scene_file(const headless_editor &editor);

    /// <summary>
    /// The lines of 'text' that are not empty, in order (one NDJSON record each in a runner's output).
    /// </summary>
    [[nodiscard]] std::vector<std::string> non_empty_lines(std::string_view text);

    /// <summary>
    /// A file of JSON lines (the audit log, an NDJSON output) as one parsed document per non-empty line; fails
    /// the case when the file cannot be read or a line does not parse.
    /// </summary>
    [[nodiscard]] std::vector<conjure::core::json::document> read_json_lines(const std::filesystem::path &path);

    /// <summary>
    /// The arguments of script.write for a file under scripts/: {path, source}.
    /// </summary>
    [[nodiscard]] conjure::core::json::document script_write_args(std::string_view path, std::string_view source);

    /// <summary>
    /// Every file under the project root except .conjure/ as path -> bytes, with project.json normalised through
    /// engine::project, for the inverse round trips of asset, script and project commands. Directories are not
    /// listed, so an empty folder is invisible here (asset_create_folder.json and asset_delete_folder.json opt out of
    /// the change check). Known gap, not a rule: undo of asset.create_material, asset.import, asset.restore and
    /// script.write keeps the folders those commands created.
    /// </summary>
    [[nodiscard]] std::map<std::string, std::string> project_file_snapshot(const std::filesystem::path &projectRoot);

    /// <summary>
    /// A complete editing-state snapshot: the serialised scene and the project files. (headless_editor::world_hash
    /// is a hash of the same scene text, so it would add nothing here.)
    /// </summary>
    struct editor_snapshot {
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
    ///   "error": {"code"}} , "expectAnyOf": [expect...], "roundTrip": bool, "expectChange": bool,
    ///   "editor": {"withScene": bool} }
    /// ${project} expands in the arguments and in the expected result; the subset is matched with json_contains.
    /// An undoable command must change the snapshot unless "expectChange" is false (then a "_comment" says why
    /// its effect lies outside the snapshot).
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
    /// Two markers loosen it: the string "@any" matches any value (the member exists, as for a generated guid),
    /// and an array whose first element is "@contains" matches when each of its other elements matches some
    /// element of 'actual', in any order and whatever else 'actual' holds (for registry listings).
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
    /// Writes all of 'text' to a non-blocking loopback socket of a test (editor/core/net/socket.h); fails the
    /// case when the socket breaks or does not take the bytes.
    /// </summary>
    void send_all(const conjure::editor::net::socket_handle &socket, std::string_view text);

    /// <summary>
    /// Reads from a non-blocking loopback socket of a test until 'atLeast' bytes arrived, the peer closed the
    /// connection or the attempts ran out, and returns what arrived.
    /// </summary>
    [[nodiscard]] std::string receive_some(
      const conjure::editor::net::socket_handle &socket,
      conjure::usize atLeast,
      conjure::u32 attempts = 200000
    );

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
