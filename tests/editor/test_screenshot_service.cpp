#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/testing/log_capture.h"

#include "conjure/core/image.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/json/document.h"
#include "conjure/editor/editor_context.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/editor/screenshot_service.h"
#include "conjure/engine/application.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/project.h"

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    #include "conjure/renderer/jpeg_xl.h"
    #include "conjure/renderer/texture_asset.h"
#endif

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

// screenshot_service.h (docs/design/04_editor.md 3.10, 7). A JPEG XL screenshot is checked by decoding it with the
// renderer's decoder, so those cases compile with the renderer module only.

using namespace conjure;
using namespace conjure::editor;

namespace {
    /// <summary>
    /// A pixel of the test pattern: neighbours differ in every channel, and alpha takes values from 0 to 255, so a
    /// lossless file must also keep the colour of fully transparent pixels.
    /// </summary>
    core::rgba8 pattern_pixel(u32 x, u32 y) {
        return core::rgba8{
          static_cast<u8>((x * 37u + y * 11u) % 256u),
          static_cast<u8>((x * 5u + y * 29u + 7u) % 256u),
          static_cast<u8>((x * y * 3u + 101u) % 256u),
          static_cast<u8>((x * 13u + y * 61u) % 256u),
        };
    }

    /// <summary>
    /// A backend whose capture is the test pattern at the requested size.
    /// </summary>
    class pattern_backend final : public screenshot_backend {
        public:
            u32 begun = 0;

        public:
            core::result<void> begin(const screenshot_request &request) override {
                (void) request;
                ++begun;
                return core::result<void>::success();
            }

            core::result<core::image> capture(const screenshot_request &request) override {
                core::image image(request.width, request.height);
                for (u32 y = 0; y < request.height; ++y) {
                    for (u32 x = 0; x < request.width; ++x) {
                        image.set_pixel(x, y, pattern_pixel(x, y));
                    }
                }
                return image;
            }
    };

    std::vector<std::byte> read_bytes(const std::filesystem::path &path) {
        std::ifstream input(path, std::ios::binary);
        REQUIRE(input.is_open());
        std::stringstream buffer = {};
        buffer << input.rdbuf();
        const std::string text = buffer.str();
        return std::vector<std::byte>(
          reinterpret_cast<const std::byte *>(text.data()),
          reinterpret_cast<const std::byte *>(text.data()) + text.size()
        );
    }

    /// <summary>
    /// A backend whose frames the test draws by hand: frame_rendered() answers true once per 'drawn = true'.
    /// It refuses the requests for the camera named in 'refusedCamera' and remembers, in order, the paths of
    /// the requests it began and captured.
    /// </summary>
    class staged_backend final : public screenshot_backend {
        public:
            u32 begun = 0;
            u32 prepared = 0;
            u32 captured = 0;
            u32 abandoned = 0;
            bool drawn = false;
            std::string refusedCamera = {};
            std::vector<std::string> begunPaths = {};
            std::vector<std::string> capturedPaths = {};

        public:
            core::result<void> begin(const screenshot_request &request) override {
                if (!refusedCamera.empty() && request.camera == refusedCamera) {
                    return make_editor_error(protocol_error_code::COMPONENT_NOT_FOUND, "no such camera");
                }
                ++begun;
                begunPaths.push_back(request.path.generic_string());
                return core::result<void>::success();
            }

            void prepare_frame(const screenshot_request &request) override {
                (void) request;
                ++prepared;
            }

            bool frame_rendered(const screenshot_request &request) override {
                (void) request;
                return std::exchange(drawn, false);
            }

            core::result<core::image> capture(const screenshot_request &request) override {
                ++captured;
                capturedPaths.push_back(request.path.generic_string());
                core::image image(request.width, request.height);
                image.fill(core::rgba8{9, 9, 9, 255});
                return image;
            }

            void abandon() noexcept override {
                ++abandoned;
            }
    };
}

TEST_SUITE("editor/screenshot_service") {

    TEST_CASE("view.screenshot - on the null device the answer is NOT_AVAILABLE") {
        editor_test::headless_editor editor = {};
        CHECK_FALSE(editor.context().screenshots().available());
        CHECK(screenshot_service::create_renderer_backend(editor.context()) == nullptr);
        const protocol_response response = editor.send(
          "view.screenshot",
          core::json::object({{"path", (editor.project_directory() / "shot.png").generic_string()}})
        );
        REQUIRE_FALSE(response.ok);
        CHECK(response.errorCode == protocol_error_code::NOT_AVAILABLE);
        CHECK_FALSE(std::filesystem::exists(editor.project_directory() / "shot.png"));
    }

    TEST_CASE("screenshot_service - a fake backend captures after the wait, writes a PNG and replies") {
        editor_test::headless_editor editor = {};
        editor.context().screenshots().set_backend(
          std::make_unique<memory_screenshot_backend>(core::rgba8{10, 20, 30, 255})
        );
        CHECK(editor.context().screenshots().available());
        const std::filesystem::path path = editor.project_directory() / "shots" / "frame.png";
        const core::json::document result = editor.ok(
          "view.screenshot",
          core::json::object({{"path", path.generic_string()}, {"width", 16}, {"height", 8}, {"waitFrames", 2}})
        );
        CHECK(result.root()["width"].as_i64() == 16);
        CHECK(result.root()["height"].as_i64() == 8);
        CHECK(result.root()["path"].as_string() == path.generic_string());
        CHECK(result.root().contains("frame"));
        CHECK(result.root()["format"].as_string() == "png");
        REQUIRE(std::filesystem::is_regular_file(path));
        const core::result<core::image> decoded = core::image::load_png(read_bytes(path));
        CONJURE_REQUIRE_OK(decoded);
        CHECK(decoded.value().width() == 16);
        CHECK(decoded.value().height() == 8);
        CHECK(decoded.value().pixel(3, 3) == core::rgba8{10, 20, 30, 255});
        CHECK(editor.context().screenshots().completed_count() == 1);
        CHECK(editor.context().screenshots().pending_count() == 0);
        CHECK(editor.context().screenshots().encoding_count() == 0);
    }

    TEST_CASE("screenshot_service - the PNG is encoded off the main thread and the reply arrives through a frame") {
        editor_test::headless_editor editor = {};
        screenshot_service &service = editor.context().screenshots();
        service.set_backend(std::make_unique<memory_screenshot_backend>(core::rgba8{1, 2, 3, 255}));
        std::optional<protocol_response> captured = std::nullopt;
        const u32 client = editor.context().events().register_client({});
        editor.context().register_response_sink(client, [&captured](const protocol_response &response) {
            captured = response;
        });
        protocol_request request = {};
        request.id = make_id(3);
        request.command = "view.screenshot";
        request.args = core::json::object({
          {"path", (editor.project_directory() / "job.png").generic_string()},
          {"width", 64},
          {"height", 64},
        });
        request.clientId = client;
        CHECK_FALSE(editor.context().dispatcher().dispatch(request).has_value());
        CHECK(service.pending_count() == 1);
        // tick() captures on the main thread and hands the image to a job; the completion lands on a later
        // drain of the main-thread queue, so pending drops before completed rises.
        service.tick();
        CHECK(service.pending_count() == 0);
        for (u32 frame = 0; frame < 1000 && !captured.has_value(); ++frame) {
            editor.pump(1);
        }
        REQUIRE(captured.has_value());
        CHECK(captured->ok);
        CHECK(captured->result.root()["width"].as_i64() == 64);
        CHECK(service.completed_count() == 1);
        CHECK(service.encoding_count() == 0);
        CHECK(std::filesystem::is_regular_file(editor.project_directory() / "job.png"));
        editor.context().unregister_response_sink(client);
        editor.context().events().unregister_client(client);
    }

    TEST_CASE("screenshot_service - cancel_pending fails the waiting requests with CANCELLED") {
        editor_test::headless_editor editor = {};
        screenshot_service &service = editor.context().screenshots();
        service.set_backend(std::make_unique<memory_screenshot_backend>());
        std::optional<protocol_response> captured = std::nullopt;
        const u32 client = editor.context().events().register_client({});
        editor.context().register_response_sink(client, [&captured](const protocol_response &response) {
            captured = response;
        });
        protocol_request request = {};
        request.id = make_id(4);
        request.command = "view.screenshot";
        request.args = core::json::object({
          {"path", (editor.project_directory() / "never.png").generic_string()},
          {"waitFrames", 1000},
        });
        request.clientId = client;
        CHECK_FALSE(editor.context().dispatcher().dispatch(request).has_value());
        CHECK(service.pending_count() == 1);
        service.cancel_pending();
        CHECK(service.pending_count() == 0);
        editor.pump(1);
        REQUIRE(captured.has_value());
        CHECK(captured->errorCode == protocol_error_code::CANCELLED);
        CHECK_FALSE(std::filesystem::exists(editor.project_directory() / "never.png"));
        editor.context().unregister_response_sink(client);
        editor.context().events().unregister_client(client);
    }

    TEST_CASE("screenshot_service::parse_request - validates every argument") {
        core::json::document args = core::json::object({{"path", "a.png"}});
        core::result<screenshot_request> parsed = screenshot_service::parse_request(args.root());
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().width == 1280);
        CHECK(parsed.value().height == 720);
        CHECK(parsed.value().camera == "editor");
        CHECK(parsed.value().format == screenshot_format::PNG);
        CHECK(parsed.value().waitFrames == 0);
        CONJURE_CHECK_ERROR(
          screenshot_service::parse_request(core::json::object({{"path", ""}}).root()),
          protocol_error_code::INVALID_ARGS
        );
        CONJURE_CHECK_ERROR(
          screenshot_service::parse_request(core::json::object({{"width", 4}}).root()),
          protocol_error_code::INVALID_ARGS
        );
        CONJURE_CHECK_ERROR(
          screenshot_service::parse_request(core::json::object({{"path", "a.png"}, {"width", 0}}).root()),
          protocol_error_code::INVALID_ARGS
        );
        CONJURE_CHECK_ERROR(
          screenshot_service::parse_request(core::json::object({{"path", "a.png"}, {"height", 100000}}).root()),
          protocol_error_code::INVALID_ARGS
        );
        // Any other camera names an entity (guid, path or name) that view.screenshot resolves; only "" is refused.
        CONJURE_CHECK_ERROR(
          screenshot_service::parse_request(core::json::object({{"path", "a.png"}, {"camera", ""}}).root()),
          protocol_error_code::INVALID_ARGS
        );
        const core::result<screenshot_request> named = screenshot_service::parse_request(
          core::json::object({{"path", "a.png"}, {"camera", "left"}}).root()
        );
        CONJURE_REQUIRE_OK(named);
        CHECK(named.value().camera == "left");
        CONJURE_CHECK_ERROR(
          screenshot_service::parse_request(core::json::object({{"path", "a.png"}, {"format", "bmp"}}).root()),
          protocol_error_code::INVALID_ARGS
        );
        parsed = screenshot_service::parse_request(core::json::object({
          {"path", "b.png"},
          {"camera", "game"},
          {"waitFrames", 5},
        }).root());
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().camera == "game");
        CHECK(parsed.value().waitFrames == 5);
    }

    TEST_CASE("view.screenshot - a camera guid must name a live entity") {
        editor_test::headless_editor editor = {};
        editor.context().screenshots().set_backend(std::make_unique<memory_screenshot_backend>());
        const std::string guid = editor.create_entity("Cam");
        (void) editor.ok("component.add", core::json::object({{"entity", guid}, {"type", "camera"}}));
        const std::filesystem::path path = editor.project_directory() / "cam.png";
        (void) editor.ok(
          "view.screenshot",
          core::json::object({{"path", path.generic_string()}, {"camera", guid}, {"width", 4}, {"height", 4}})
        );
        CHECK(std::filesystem::is_regular_file(path));
        const std::string missing = core::uuid::generate().to_string();
        CHECK(editor.fail(
          "view.screenshot",
          core::json::object({{"path", path.generic_string()}, {"camera", missing}})
        ) == protocol_error_code::ENTITY_NOT_FOUND);
    }

    TEST_CASE("regression - F653 view.screenshot resolves a camera named by path and needs a camera component") {
        editor_test::headless_editor editor = {};
        editor.context().screenshots().set_backend(std::make_unique<memory_screenshot_backend>());
        const std::string guid = editor.create_entity("Cam");
        (void) editor.ok("component.add", core::json::object({{"entity", guid}, {"type", "camera"}}));
        (void) editor.create_entity("Plain");
        const std::filesystem::path path = editor.project_directory() / "by_path.png";
        (void) editor.ok(
          "view.screenshot",
          core::json::object({{"path", path.generic_string()}, {"camera", "Cam"}, {"width", 4}, {"height", 4}})
        );
        CHECK(std::filesystem::is_regular_file(path));
        CHECK(editor.fail(
          "view.screenshot",
          core::json::object({{"path", path.generic_string()}, {"camera", "Plain"}})
        ) == protocol_error_code::COMPONENT_NOT_FOUND);
        CHECK(editor.fail(
          "view.screenshot",
          core::json::object({{"path", path.generic_string()}, {"camera", "Nobody"}})
        ) == protocol_error_code::ENTITY_NOT_FOUND);
        CHECK(editor.fail(
          "view.screenshot",
          core::json::object({{"path", path.generic_string()}, {"camera", ""}})
        ) == protocol_error_code::INVALID_ARGS);
    }

    TEST_CASE("regression - F653 view.screenshot under --offscreen without a device does not suggest --offscreen") {
        testing::log_capture log = {};
        // An --offscreen session whose engine fell back to the null device: a context marked offscreen over an
        // engine without a renderer (the editor's own --offscreen start-up needs the shader library next to the
        // executable, which a unit test cannot rely on).
        engine::application_desc desc = {};
        desc.config = engine::engine_config::headless_defaults();
        desc.config.scripting.enabled = false;
        engine::application application(std::move(desc));
        CONJURE_REQUIRE_OK(application.initialise());
        // Declared after the application so it is destroyed first; the engine shuts down while the context's module
        // consumer is still alive, as editor_application::shutdown does it.
        editor_context context(application, nullptr, editor_display_mode::HEADLESS_OFFSCREEN);
        CHECK(context.offscreen());
        CHECK_FALSE(context.screenshots().available());
        protocol_request request = {};
        request.id = make_id(1);
        request.command = "view.screenshot";
        request.args = core::json::object({{"path", "shot.png"}});
        const std::optional<protocol_response> response = context.dispatcher().dispatch(request);
        application.shutdown();
        REQUIRE(response.has_value());
        REQUIRE_FALSE(response->ok);
        CHECK(response->errorCode == protocol_error_code::NOT_AVAILABLE);
        CHECK(response->errorMessage.find("--offscreen") == std::string::npos);
        CHECK(response->errorMessage.find("window") != std::string::npos);
    }

    TEST_CASE("regression - code sweep F104 the service counts a waited frame only once the backend drew one") {
        editor_test::headless_editor editor = {};
        screenshot_service &service = editor.context().screenshots();
        std::unique_ptr<staged_backend> owned = std::make_unique<staged_backend>();
        staged_backend *backend = owned.get();
        service.set_backend(std::move(owned));
        std::optional<protocol_response> captured = std::nullopt;
        const u32 client = editor.context().events().register_client({});
        editor.context().register_response_sink(client, [&captured](const protocol_response &response) {
            captured = response;
        });
        protocol_request request = {};
        request.id = make_id(9);
        request.command = "view.screenshot";
        request.args = core::json::object({
          {"path", (editor.project_directory() / "staged.png").generic_string()},
          {"width", 8},
          {"height", 8},
          {"waitFrames", 1},
        });
        request.clientId = client;
        CHECK_FALSE(editor.context().dispatcher().dispatch(request).has_value());
        CHECK(backend->begun == 1);

        // Nothing drawn yet, however often the service is ticked: the request waits and keeps asking for its
        // frame. It used to be captured by the first tick, in the frame it arrived in.
        service.tick();
        service.tick();
        CHECK(backend->captured == 0);
        CHECK(backend->prepared == 2);
        CHECK(service.pending_count() == 1);

        // One drawn frame is the frame the request itself asked for; waitFrames 1 wants one more.
        backend->drawn = true;
        service.tick();
        CHECK(backend->captured == 0);
        CHECK(backend->prepared == 3);
        service.tick();
        CHECK(backend->captured == 0);
        backend->drawn = true;
        service.tick();
        CHECK(backend->captured == 1);
        CHECK(service.pending_count() == 0);
        for (u32 frame = 0; frame < 1000 && !captured.has_value(); ++frame) {
            editor.pump(1);
        }
        REQUIRE(captured.has_value());
        CHECK(captured->ok);

        // Cancelling tells the backend to let go without touching the engine.
        captured.reset();
        request.id = make_id(10);
        CHECK_FALSE(editor.context().dispatcher().dispatch(request).has_value());
        CHECK(backend->abandoned == 0);
        service.cancel_pending();
        CHECK(backend->abandoned == 1);
        CHECK(service.pending_count() == 0);
        editor.context().unregister_response_sink(client);
        editor.context().events().unregister_client(client);
    }

    TEST_CASE("regression - code sweep F104 screenshots asked for together are taken one at a time, in order") {
        editor_test::headless_editor editor = {};
        screenshot_service &service = editor.context().screenshots();
        std::unique_ptr<staged_backend> owned = std::make_unique<staged_backend>();
        staged_backend *backend = owned.get();
        backend->refusedCamera = "game";
        service.set_backend(std::move(owned));
        std::vector<protocol_response> answers = {};
        const u32 client = editor.context().events().register_client({});
        editor.context().register_response_sink(client, [&answers](const protocol_response &response) {
            answers.push_back(response);
        });
        const std::string first = (editor.project_directory() / "first.png").generic_string();
        const std::string refused = (editor.project_directory() / "refused.png").generic_string();
        const std::string third = (editor.project_directory() / "third.png").generic_string();
        const auto ask = [&](i64 id, const std::string &path, const std::string &camera) {
            protocol_request request = {};
            request.id = make_id(id);
            request.command = "view.screenshot";
            request.args = core::json::object({{"path", path}, {"width", 4}, {"height", 4}, {"camera", camera}});
            request.clientId = client;
            CHECK_FALSE(editor.context().dispatcher().dispatch(request).has_value());
        };
        // Three requests in one frame, as a client that pipelines them sends them. Only the first is with the
        // backend: all of them used to be begun at once into the backend's one target, and the first was
        // answered with the picture of the last.
        ask(21, first, "editor");
        ask(22, refused, "game");
        ask(23, third, "editor");
        CHECK(service.pending_count() == 3);
        CHECK(backend->begunPaths == std::vector<std::string>{first});

        // Nothing drawn: the first keeps waiting and the others keep their place.
        service.tick();
        CHECK(backend->captured == 0);
        CHECK(backend->begun == 1);
        CHECK(service.pending_count() == 3);

        // The first is captured; in the same tick the second is begun and refused, and the third is begun and
        // waits for a frame of its own.
        backend->drawn = true;
        service.tick();
        CHECK(backend->capturedPaths == std::vector<std::string>{first});
        CHECK(backend->begunPaths == std::vector<std::string>{first, third});
        CHECK(service.pending_count() == 1);
        service.tick();
        CHECK(backend->captured == 1);

        backend->drawn = true;
        service.tick();
        CHECK(backend->capturedPaths == std::vector<std::string>{first, third});
        CHECK(service.pending_count() == 0);
        for (u32 frame = 0; frame < 1000 && answers.size() < 3; ++frame) {
            editor.pump(1);
        }
        REQUIRE(answers.size() == 3);
        u32 succeeded = 0;
        for (const protocol_response &answer : answers) {
            if (answer.ok) {
                ++succeeded;
                continue;
            }
            CHECK(answer.id.root().as_i64() == 22);
            CHECK(answer.errorCode == protocol_error_code::COMPONENT_NOT_FOUND);
        }
        CHECK(succeeded == 2);
        CHECK(std::filesystem::is_regular_file(first));
        CHECK(std::filesystem::is_regular_file(third));
        CHECK_FALSE(std::filesystem::exists(refused));
        editor.context().unregister_response_sink(client);
        editor.context().events().unregister_client(client);
    }

    TEST_CASE("screenshot_format - wire names round trip and a .png or .jxl extension names its format") {
        CHECK(to_string(screenshot_format::PNG) == "png");
        CHECK(to_string(screenshot_format::JPEG_XL) == "jxl");
        CHECK(parse_screenshot_format("png") == screenshot_format::PNG);
        CHECK(parse_screenshot_format("jxl") == screenshot_format::JPEG_XL);
        // Wire names are exact, like every other enumeration of the protocol.
        CHECK_FALSE(parse_screenshot_format("JXL").has_value());
        CHECK_FALSE(parse_screenshot_format("jpeg-xl").has_value());
        CHECK_FALSE(parse_screenshot_format("bmp").has_value());
        CHECK_FALSE(parse_screenshot_format("").has_value());

        // Extensions are matched in any case; only the file name's own extension counts.
        CHECK(screenshot_format_of("shots/frame.png") == screenshot_format::PNG);
        CHECK(screenshot_format_of("FRAME.PNG") == screenshot_format::PNG);
        CHECK(screenshot_format_of("shots/frame.jxl") == screenshot_format::JPEG_XL);
        CHECK(screenshot_format_of("C:/shots/Frame.JxL") == screenshot_format::JPEG_XL);
        CHECK_FALSE(screenshot_format_of("frame.bmp").has_value());
        CHECK_FALSE(screenshot_format_of("frame").has_value());
        CHECK_FALSE(screenshot_format_of("frame.jxl.part").has_value());
        CHECK_FALSE(screenshot_format_of("shots.jxl/frame").has_value());
        CHECK_FALSE(screenshot_format_of(".jxl").has_value());
        CHECK_FALSE(screenshot_format_of("").has_value());
    }

    TEST_CASE("screenshot_service::parse_request - the path's extension picks the format unless format names it") {
        const auto format_of = [](core::json::document args) {
            const core::result<screenshot_request> parsed = screenshot_service::parse_request(args.root());
            REQUIRE_MESSAGE(parsed.has_value(), testing::describe_error(parsed));
            return parsed.value().format;
        };
        // Without "format": .jxl is JPEG XL, everything else (a .png, another extension, none) the default PNG.
        CHECK(format_of(core::json::object({{"path", "a.jxl"}})) == screenshot_format::JPEG_XL);
        CHECK(format_of(core::json::object({{"path", "A.JXL"}})) == screenshot_format::JPEG_XL);
        CHECK(format_of(core::json::object({{"path", "a.png"}})) == screenshot_format::PNG);
        CHECK(format_of(core::json::object({{"path", "a.bmp"}})) == screenshot_format::PNG);
        CHECK(format_of(core::json::object({{"path", "a"}})) == screenshot_format::PNG);
        // A named format wins over an extension that names none, and may repeat the one it names.
        CHECK(format_of(core::json::object({{"path", "a.bin"}, {"format", "jxl"}})) == screenshot_format::JPEG_XL);
        CHECK(format_of(core::json::object({{"path", "a"}, {"format", "png"}})) == screenshot_format::PNG);
        CHECK(format_of(core::json::object({{"path", "a.jxl"}, {"format", "jxl"}})) == screenshot_format::JPEG_XL);
        CHECK(format_of(core::json::object({{"path", "a.png"}, {"format", "png"}})) == screenshot_format::PNG);

        // An unknown or misspelt format, or one that is not a string.
        for (const core::json::document &args : {
               core::json::object({{"path", "a.png"}, {"format", "bmp"}}),
               core::json::object({{"path", "a.jxl"}, {"format", "JXL"}}),
               core::json::object({{"path", "a"}, {"format", ""}}),
               core::json::object({{"path", "a"}, {"format", 7}}),
             }) {
            const core::result<screenshot_request> refused = screenshot_service::parse_request(args.root());
            CONJURE_CHECK_ERROR(refused, protocol_error_code::INVALID_ARGS);
            if (!refused.has_value()) {
                CHECK(refused.get_error().message().find("args.format") != std::string::npos);
            }
        }
        // A format the path's .png or .jxl extension contradicts: the file would carry the other format's bytes.
        const core::result<screenshot_request> pngAsJxl = screenshot_service::parse_request(
          core::json::object({{"path", "shot.png"}, {"format", "jxl"}}).root()
        );
        REQUIRE(testing::has_error(pngAsJxl, protocol_error_code::INVALID_ARGS));
        CHECK(pngAsJxl.get_error().message().find("\".png\"") != std::string::npos);
        CONJURE_CHECK_ERROR(
          screenshot_service::parse_request(core::json::object({{"path", "shot.JXL"}, {"format", "png"}}).root()),
          protocol_error_code::INVALID_ARGS
        );
    }

    TEST_CASE("view.screenshot - the schema lists png and jxl and an unknown format is refused before any capture") {
        editor_test::headless_editor editor = {};
        std::unique_ptr<pattern_backend> owned = std::make_unique<pattern_backend>();
        pattern_backend *backend = owned.get();
        editor.context().screenshots().set_backend(std::move(owned));

        const core::json::document descriptor = editor.ok(
          "editor.schema",
          core::json::object({{"command", "view.screenshot"}})
        );
        const core::json::value allowed = descriptor.root()["args"]["properties"]["format"]["enum"];
        REQUIRE(allowed.size() == 2);
        CHECK(allowed[0].as_string() == "png");
        CHECK(allowed[1].as_string() == "jxl");
        CHECK(descriptor.root()["result"]["properties"].contains("format"));

        const std::filesystem::path path = editor.project_directory() / "shot.webp";
        for (const core::json::document &args : {
               core::json::object({{"path", path.generic_string()}, {"format", "webp"}}),
               core::json::object({{"path", path.generic_string()}, {"format", "JXL"}}),
               core::json::object({{"path", path.generic_string()}, {"format", 3}}),
             }) {
            const protocol_response response = editor.send("view.screenshot", args);
            REQUIRE_FALSE(response.ok);
            CHECK(response.errorCode == protocol_error_code::INVALID_ARGS);
            CHECK(response.errorMessage.find("args.format") != std::string::npos);
        }
        CHECK(backend->begun == 0);
        CHECK_FALSE(std::filesystem::exists(path));
    }

    TEST_CASE("screenshot_service::supports - PNG always, JPEG XL when the renderer module is built") {
        CHECK(screenshot_service::supports(screenshot_format::PNG));
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
        CHECK(screenshot_service::supports(screenshot_format::JPEG_XL));
#else
        // The encoder lives in the renderer: without it a JPEG XL request is refused before anything is captured.
        CHECK_FALSE(screenshot_service::supports(screenshot_format::JPEG_XL));
        editor_test::headless_editor editor = {};
        std::unique_ptr<pattern_backend> owned = std::make_unique<pattern_backend>();
        pattern_backend *backend = owned.get();
        editor.context().screenshots().set_backend(std::move(owned));
        const std::filesystem::path path = editor.project_directory() / "shot.jxl";
        CHECK(editor.fail(
          "view.screenshot",
          core::json::object({{"path", path.generic_string()}})
        ) == protocol_error_code::NOT_AVAILABLE);
        CHECK(backend->begun == 0);
        CHECK_FALSE(std::filesystem::exists(path));
#endif
    }

    TEST_CASE("screenshot_service - a screenshot path that cannot be written answers IO_ERROR") {
        editor_test::headless_editor editor = {};
        editor.context().screenshots().set_backend(std::make_unique<pattern_backend>());
        // A directory stands where the file should go.
        std::vector<std::string> names = {"taken.png"};
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
        names.emplace_back("taken.jxl");
#endif
        for (const std::string &name : names) {
            INFO(name);
            const std::filesystem::path path = editor.project_directory() / name;
            std::error_code error = {};
            REQUIRE(std::filesystem::create_directories(path, error));
            const protocol_response response = editor.send(
              "view.screenshot",
              core::json::object({{"path", path.generic_string()}, {"width", 8}, {"height", 8}})
            );
            REQUIRE_FALSE(response.ok);
            CHECK(response.errorCode == protocol_error_code::IO_ERROR);
            CHECK(response.errorMessage.find(name) != std::string::npos);
            CHECK(std::filesystem::is_directory(path));
        }
        CHECK(editor.context().screenshots().completed_count() == 0);
        CHECK(editor.context().screenshots().encoding_count() == 0);
    }

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER

    TEST_CASE("screenshot_service - a .jxl screenshot is lossless: it decodes to the pixels the .png one holds") {
        constexpr u32 WIDTH = 40;
        constexpr u32 HEIGHT = 24;
        editor_test::headless_editor editor = {};
        editor.context().screenshots().set_backend(std::make_unique<pattern_backend>());
        const std::filesystem::path pngPath = editor.project_directory() / "shots" / "frame.png";
        const std::filesystem::path jxlPath = editor.project_directory() / "shots" / "frame.jxl";
        const core::json::document pngResult = editor.ok(
          "view.screenshot",
          core::json::object({{"path", pngPath.generic_string()}, {"width", WIDTH}, {"height", HEIGHT}})
        );
        const core::json::document jxlResult = editor.ok(
          "view.screenshot",
          core::json::object({{"path", jxlPath.generic_string()}, {"width", WIDTH}, {"height", HEIGHT}})
        );
        CHECK(pngResult.root()["format"].as_string() == "png");
        CHECK(jxlResult.root()["format"].as_string() == "jxl");
        CHECK(jxlResult.root()["path"].as_string() == jxlPath.generic_string());
        CHECK(jxlResult.root()["width"].as_i64() == WIDTH);
        CHECK(jxlResult.root()["height"].as_i64() == HEIGHT);
        CHECK(editor.context().screenshots().completed_count() == 2);

        const std::vector<std::byte> pngBytes = read_bytes(pngPath);
        const std::vector<std::byte> jxlBytes = read_bytes(jxlPath);
        CHECK_FALSE(renderer::is_jpeg_xl(pngBytes));
        REQUIRE(renderer::is_jpeg_xl(jxlBytes));
        const core::result<core::image> png = core::image::load_png(pngBytes);
        CONJURE_REQUIRE_OK(png);
        // SRGB: the file is tagged sRGB, so the decoder hands its samples over untouched.
        const core::result<renderer::texture_asset> jxl =
          renderer::texture_asset::decode(jxlBytes, renderer::texture_colour_space::SRGB, false);
        CONJURE_REQUIRE_OK(jxl);
        REQUIRE(png.value().width() == WIDTH);
        REQUIRE(png.value().height() == HEIGHT);
        REQUIRE(jxl.value().width() == WIDTH);
        REQUIRE(jxl.value().height() == HEIGHT);
        u32 pngMismatches = 0;
        u32 jxlMismatches = 0;
        for (u32 y = 0; y < HEIGHT; ++y) {
            for (u32 x = 0; x < WIDTH; ++x) {
                pngMismatches += png.value().pixel(x, y) == pattern_pixel(x, y) ? 0 : 1;
                jxlMismatches += jxl.value().pixel(x, y) == png.value().pixel(x, y) ? 0 : 1;
            }
        }
        CHECK(pngMismatches == 0);
        CHECK(jxlMismatches == 0);
    }

    TEST_CASE("screenshot_service - format jxl writes the same JPEG XL file inline and on the job system") {
        const auto shoot = [](std::vector<std::string> engineArguments, u32 expectedWorkers) {
            editor_test::headless_editor_options options = {};
            options.engineArguments = std::move(engineArguments);
            editor_test::headless_editor editor(options);
            const core::jobs::job_system *jobs = editor.context().engine_context().jobs;
            REQUIRE(jobs != nullptr);
            CHECK(jobs->worker_count() == expectedWorkers);
            editor.context().screenshots().set_backend(std::make_unique<pattern_backend>());
            // An extension that names no format: the argument decides.
            const std::filesystem::path path = editor.project_directory() / "frame.bin";
            const core::json::document result = editor.ok(
              "view.screenshot",
              core::json::object({{"path", path.generic_string()}, {"width", 130}, {"height", 70}, {"format", "jxl"}})
            );
            CHECK(result.root()["format"].as_string() == "jxl");
            CHECK(editor.context().screenshots().encoding_count() == 0);
            return read_bytes(path);
        };
        // The headless default runs the encode on a job with two workers; --jobs 0 encodes inline on the main thread.
        const std::vector<std::byte> onJobs = shoot({}, 2);
        const std::vector<std::byte> inlined = shoot({"--jobs", "0"}, 0);
        REQUIRE(renderer::is_jpeg_xl(onJobs));
        CHECK(onJobs == inlined);
        const core::result<renderer::texture_asset> decoded =
          renderer::texture_asset::decode(inlined, renderer::texture_colour_space::SRGB, false);
        CONJURE_REQUIRE_OK(decoded);
        CHECK(decoded.value().pixel(129, 69) == pattern_pixel(129, 69));
        CHECK(decoded.value().pixel(0, 0) == pattern_pixel(0, 0));
    }

    TEST_CASE("regression - code sweep F187 a relative .jxl path is written under the project root as JPEG XL") {
        editor_test::headless_editor editor = {};
        editor.context().screenshots().set_backend(std::make_unique<pattern_backend>());
        // The extension picks the format; the path is then made absolute against the project root (D24).
        const core::json::document result = editor.ok(
          "view.screenshot",
          core::json::object({{"path", "shots/relative.jxl"}, {"width", 8}, {"height", 8}})
        );
        const std::filesystem::path expected = editor.context().project()->root_directory() / "shots" / "relative.jxl";
        CHECK(result.root()["format"].as_string() == "jxl");
        CHECK(std::filesystem::path(std::string(result.root()["path"].as_string())) == expected);
        const std::vector<std::byte> bytes = read_bytes(expected);
        REQUIRE(renderer::is_jpeg_xl(bytes));
        const core::result<renderer::texture_asset> decoded =
          renderer::texture_asset::decode(bytes, renderer::texture_colour_space::SRGB, false);
        CONJURE_REQUIRE_OK(decoded);
        CHECK(decoded.value().pixel(0, 0) == pattern_pixel(0, 0));
        CHECK(decoded.value().pixel(7, 7) == pattern_pixel(7, 7));
    }

#endif

    TEST_CASE("memory_screenshot_backend - counts begins and captures") {
        memory_screenshot_backend backend(core::rgba8{1, 2, 3, 4});
        screenshot_request request = {};
        request.path = "x.png";
        request.width = 2;
        request.height = 2;
        CONJURE_REQUIRE_OK(backend.begin(request));
        const core::result<core::image> image = backend.capture(request);
        CONJURE_REQUIRE_OK(image);
        CHECK(image.value().pixel(1, 1) == core::rgba8{1, 2, 3, 4});
        CHECK(backend.begun() == 1);
        CHECK(backend.captured() == 1);
    }

    TEST_CASE("regression - code sweep F187 view.screenshot writes a relative path under the project root") {
        editor_test::headless_editor editor = {};
        editor.context().screenshots().set_backend(
          std::make_unique<memory_screenshot_backend>(core::rgba8{10, 20, 30, 255})
        );
        const core::json::document result = editor.ok(
          "view.screenshot",
          core::json::object({{"path", "shots/relative.png"}, {"width", 8}, {"height", 8}})
        );
        const std::filesystem::path expected = editor.context().project()->root_directory() / "shots" / "relative.png";
        CHECK(std::filesystem::path(std::string(result.root()["path"].as_string())) == expected);
        CHECK(std::filesystem::is_regular_file(expected));
    }
}
