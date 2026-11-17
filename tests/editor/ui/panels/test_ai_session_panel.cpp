#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/editor/protocol/protocol_server.h"
#include "conjure/editor/ui/imgui_frame_scope.h"
#include "conjure/editor/ui/panels/ai_session_panel.h"

#include <string>

// ai_session_panel.h: an in-process agent session through the loopback transport.

using namespace conjure;
using namespace conjure::editor;
using namespace conjure::editor::ui;

TEST_SUITE("editor/ui/panels/ai_session_panel") {

    TEST_CASE("ai_session_panel - constructs closed and disconnected") {
        ai_session_panel panel = {};
        CHECK(panel.title() == "AI Session");
        CHECK_FALSE(panel.is_open());
        CHECK_FALSE(panel.is_connected());
        CHECK(panel.sent_count() == 0);
        CHECK(panel.transcript().empty());
    }

    TEST_CASE("ai_session_panel - send connects through the server and the response lands in the transcript") {
        editor_test::headless_editor editor = {};
        ai_session_panel panel = {};
        panel.send(editor.context(), "editor.state");
        CHECK(panel.is_connected());
        CHECK(panel.sent_count() == 1);
        editor.pump(2);
        panel.update(editor.context());
        bool sawResponse = false;
        for (const std::string &line : panel.transcript()) {
            sawResponse = sawResponse || (
              line.find("\"ok\":true") != std::string::npos && line.find("playState") != std::string::npos
            );
        }
        CHECK(sawResponse);
        panel.send(editor.context(), R"({"id":"raw","cmd":"session.ping"})");
        editor.pump(2);
        panel.update(editor.context());
        bool sawPing = false;
        for (const std::string &line : panel.transcript()) {
            sawPing = sawPing || line.find("\"id\":\"raw\"") != std::string::npos;
        }
        CHECK(sawPing);
        panel.set_open(true);
        imgui_frame_scope scope{};
        scope.begin_frame();
        panel.draw(editor.context());
        scope.end_frame();
        CHECK(panel.transcript().size() <= ai_session_panel::MAX_TRANSCRIPT_LINES);
    }

    TEST_CASE("regression - panels share the server's loopback transport instead of adding one per connection") {
        editor_test::headless_editor editor = {};
        protocol_server &server = editor.application().server();
        const usize before = server.transport_count();
        ai_session_panel first = {};
        first.send(editor.context(), "session.ping");
        CHECK(server.transport_count() == before + 1);
        {
            ai_session_panel second = {};
            second.send(editor.context(), "session.ping");
            CHECK(server.transport_count() == before + 1);
            editor.pump(2);
            second.update(editor.context());
            CHECK(second.is_connected());
        }
        // The second panel disconnected on destruction; a new one reconnects through the same transport.
        editor.pump(2);
        ai_session_panel third = {};
        third.send(editor.context(), "session.ping");
        editor.pump(2);
        third.update(editor.context());
        CHECK(third.is_connected());
        CHECK(server.transport_count() == before + 1);
        first.update(editor.context());
        CHECK(first.is_connected());
    }

    TEST_CASE("regression - with an AI token set the panel says hello before its first command") {
        editor_test::headless_editor editor = {};
        CONJURE_REQUIRE_OK(editor.application().server().set_token("secret"));
        REQUIRE(editor.application().server().token() == "secret");
        ai_session_panel panel = {};
        panel.send(editor.context(), "editor.state");
        CHECK(panel.sent_count() == 1);
        editor.pump(2);
        panel.update(editor.context());
        CHECK(panel.is_connected());
        bool sawState = false;
        bool sawHello = false;
        for (const std::string &line : panel.transcript()) {
            CAPTURE(line);
            sawState = sawState || (
              line.find("\"ok\":true") != std::string::npos && line.find("playState") != std::string::npos
            );
            sawHello = sawHello || line.find("ai_session_panel.hello") != std::string::npos;
            CHECK(line.find("UNAUTHORISED") == std::string::npos);
            CHECK(line.find("secret") == std::string::npos);
        }
        CHECK(sawState);
        CHECK(sawHello);
    }

    TEST_CASE("ai_session_panel - with only an inspect-only token set the panel still has full control") {
        editor_test::headless_editor editor = {};
        protocol_server &server = editor.application().server();
        // The explicit inspect token gates the loopback; the panel is the user at the editor, not an
        // inspect-only client, and presents the full-control token the server generated.
        CONJURE_REQUIRE_OK(server.set_inspect_token("look-51d0e2"));
        const std::string generated = server.token();
        REQUIRE(generated.size() == ACCESS_TOKEN_LENGTH);
        ai_session_panel panel = {};
        panel.send(editor.context(), R"({"id":7,"cmd":"entity.create","args":{"name":"From the panel"}})");
        editor.pump(2);
        panel.update(editor.context());
        CHECK(panel.is_connected());
        bool sawFullControl = false;
        for (const std::string &line : panel.transcript()) {
            CAPTURE(line);
            sawFullControl = sawFullControl || line.find(R"("access":"full_control")") != std::string::npos;
            CHECK(line.find("UNAUTHORISED") == std::string::npos);
            CHECK(line.find(generated) == std::string::npos);
            CHECK(line.find("look-51d0e2") == std::string::npos);
        }
        CHECK(sawFullControl);
        CHECK(editor.scene().world().entity_count() == 1);
    }

    TEST_CASE("ai_session_panel - the transcript is capped") {
        editor_test::headless_editor editor = {};
        ai_session_panel panel = {};
        for (usize index = 0; index < ai_session_panel::MAX_TRANSCRIPT_LINES + 20; ++index) {
            panel.send(editor.context(), "session.ping");
        }
        editor.pump(3);
        panel.update(editor.context());
        CHECK(panel.transcript().size() == ai_session_panel::MAX_TRANSCRIPT_LINES);
    }

    TEST_CASE("regression - code sweep F187 the panel opens with session.hello, so a mistyped first line is "
      "answered instead of ending the session") {
        editor_test::headless_editor editor = {};
        REQUIRE(editor.application().server().token().empty());
        ai_session_panel panel = {};
        // The very first thing typed is not a request: the server would disconnect a client that opened with it.
        panel.send(editor.context(), "{not a request");
        editor.pump(2);
        panel.update(editor.context());
        CHECK(panel.is_connected());
        bool sawHelloAnswer = false;
        bool sawParseError = false;
        for (const std::string &line : panel.transcript()) {
            CAPTURE(line);
            sawHelloAnswer = sawHelloAnswer || (
              line.find("ai_session_panel.hello") != std::string::npos && line.find("\"ok\":true") != std::string::npos
            );
            sawParseError = sawParseError || line.find("PARSE_ERROR") != std::string::npos;
            // Without a token to present, the hello is not announced as carrying one.
            CHECK(line.find("token hidden") == std::string::npos);
        }
        CHECK(sawHelloAnswer);
        CHECK(sawParseError);

        panel.send(editor.context(), "session.ping");
        editor.pump(2);
        panel.update(editor.context());
        CHECK(panel.is_connected());
        bool sawPing = false;
        for (const std::string &line : panel.transcript()) {
            const bool answered = line.find("\"ok\":true") != std::string::npos;
            sawPing = sawPing || (answered && line.find("\"frame\"") != std::string::npos);
        }
        CHECK(sawPing);
    }
}
