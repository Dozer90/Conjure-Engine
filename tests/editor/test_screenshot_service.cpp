#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/image.h"
#include "conjure/core/json/document.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/editor/screenshot_service.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

// screenshot_service.h (docs/design/04_editor.md 3.10, 7).

using namespace conjure;
using namespace conjure::editor;

namespace {
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
        CHECK(parsed.value().format == "png");
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
        CONJURE_CHECK_ERROR(
          screenshot_service::parse_request(core::json::object({{"path", "a.png"}, {"camera", "left"}}).root()),
          protocol_error_code::INVALID_ARGS
        );
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
}
