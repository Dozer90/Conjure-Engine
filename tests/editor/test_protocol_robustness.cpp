#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/event_broadcaster.h"
#include "conjure/editor/protocol/line_splitter.h"
#include "conjure/editor/protocol/loopback_transport.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/editor/protocol/protocol_server.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Protocol robustness (docs/design/08_testing.md 6.3 item 6): the corpus under tests/editor/data/protocol_corpus/
// (truncated JSON, deep nesting, huge numbers, invalid UTF-8, null bytes, duplicate keys, a 1 MiB string, empty
// lines, ...) never crashes the server; every entry yields an error response, a valid response, or nothing. The
// corpus is sent twice: behind a first request, and as the first bytes of a connection, which is what an
// unauthenticated client controls (04_editor.md D23, D24).

using namespace conjure;
using namespace conjure::editor;

namespace {
    std::vector<std::filesystem::path> corpus_files() {
        std::vector<std::filesystem::path> files = {};
        const std::filesystem::path directory = std::filesystem::path(
          editor_test::fixture_directory()
        ).parent_path() / "protocol_corpus";
        std::error_code ignored = {};
        REQUIRE_MESSAGE(std::filesystem::is_directory(directory, ignored), directory.generic_string());
        for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(directory, ignored)) {
            if (entry.is_regular_file(ignored)) {
                files.push_back(entry.path());
            }
        }
        std::sort(files.begin(), files.end());
        return files;
    }

    /// <summary>
    /// Whether the first line of 'payload', framed as a transport frames it, is a request. It decides what
    /// the server does with a client that sends the payload as its first bytes.
    /// </summary>
    bool opens_with_request(std::string_view payload) {
        line_splitter splitter(MAX_MESSAGE_BYTES);
        splitter.feed(payload);
        splitter.feed("\n");
        std::string first = "";
        if (splitter.next_frame(first) != line_splitter::frame::LINE) {
            return false;
        }
        return parse_request(first).has_value();
    }
}

TEST_SUITE("editor/protocol_robustness") {

    TEST_CASE("protocol_robustness - the corpus has at least sixty entries") {
        CHECK(corpus_files().size() >= 60);
    }

    TEST_CASE("protocol_robustness - every corpus entry is answered or ignored, never fatal") {
        editor_test::headless_editor editor = {};
        editor.log().allow_errors();
        // command.executed is how the case sees what was dispatched; no client hears it unless subscribed.
        editor.context().events().set_subscriptions(
          editor.client_id(),
          default_subscriptions() | event_bit(editor_event::COMMAND_EXECUTED)
        );
        protocol_server &server = editor.application().server();
        std::unique_ptr<loopback_transport> loopback = std::make_unique<loopback_transport>();
        loopback_transport *transport = loopback.get();
        CONJURE_REQUIRE_OK(server.add_transport(std::move(loopback)));
        bool openWithHello = false;
        SUBCASE("after a first request, where a line that is not a request is answered and the client stays") {
            openWithHello = true;
        }
        SUBCASE("as the first bytes of a connection, the part a stranger controls before any handshake") {
            // Code sweep F187 and F740: an entry that does not open with a request ends the connection at its
            // first line (the first-line rule of 04_editor.md D24), and nothing after that line is read.
        }
        for (const std::filesystem::path &file : corpus_files()) {
            CAPTURE(file.filename().generic_string());
            const std::string payload = editor_test::read_text_file(file);
            const bool opensWithRequest = openWithHello || opens_with_request(payload);
            loopback_client client = transport->connect();
            editor.pump(1);
            REQUIRE(client.is_connected());
            editor.clear_events();
            if (openWithHello) {
                client.send(R"({"id":"hello","cmd":"session.hello"})");
            }
            client.send_raw(payload);
            client.send_raw("\n{\"id\":\"probe\",\"cmd\":\"session.ping\"}\n");
            editor.pump(3);
            const std::vector<std::string> replies = client.receive_all();
            std::vector<protocol_response> responses = {};
            bool probeAnswered = false;
            bool tooLarge = false;
            for (const std::string &line : replies) {
                core::result<protocol_response> response = parse_response(line);
                const core::result<protocol_event> event = parse_event(line);
                const bool parsed = response.has_value() || event.has_value();
                CHECK_MESSAGE(parsed, line.substr(0, 200));
                if (!response.has_value()) {
                    continue;
                }
                if (response.value().id.root().as_string("") == "probe") {
                    probeAnswered = response.value().ok;
                }
                tooLarge = tooLarge || response.value().errorCode == protocol_error_code::MESSAGE_TOO_LARGE;
                responses.push_back(std::move(response).value());
            }
            usize executed = 0;
            for (const protocol_event &event : editor.events()) {
                if (event.name == "command.executed") {
                    ++executed;
                }
            }
            if (opensWithRequest) {
                // The entry is taken line by line. Either the probe got through, or a later line of the entry
                // was oversized and the client was disconnected after a MESSAGE_TOO_LARGE answer.
                CHECK(executed >= 1);
                if (!probeAnswered) {
                    CHECK_FALSE(client.is_connected());
                    CHECK(tooLarge);
                }
            } else {
                // One answer, the reason, and the connection is closed: neither the rest of the entry nor the
                // probe behind it was read, and nothing was dispatched.
                CHECK_FALSE(probeAnswered);
                CHECK_FALSE(client.is_connected());
                CHECK(executed == 0);
                REQUIRE(responses.size() == 1);
                CHECK_FALSE(responses.front().ok);
                const protocol_error_code code = responses.front().errorCode;
                CAPTURE(to_string(code));
                const bool refusal = code == protocol_error_code::PARSE_ERROR ||
                  code == protocol_error_code::INVALID_REQUEST || code == protocol_error_code::MESSAGE_TOO_LARGE;
                CHECK(refusal);
            }
            client.disconnect();
            editor.pump(1);
        }
        // The server and the editor still work afterwards.
        (void) editor.ok("session.ping");
        (void) editor.create_entity("still alive");
    }

    TEST_CASE("protocol_robustness - garbage after a valid request does not disturb the request") {
        editor_test::headless_editor editor = {};
        editor.log().allow_errors();
        protocol_server &server = editor.application().server();
        std::unique_ptr<loopback_transport> loopback = std::make_unique<loopback_transport>();
        loopback_transport *transport = loopback.get();
        CONJURE_REQUIRE_OK(server.add_transport(std::move(loopback)));
        loopback_client client = transport->connect();
        editor.pump(1);
        static constexpr char PAYLOAD[] =
          "{\"id\":1,\"cmd\":\"editor.state\"}\n\x00\x01\x02\xff\xfe\n{\"id\":2,\"cmd\":\"session.ping\"}\n";
        client.send_raw(std::string_view(PAYLOAD, sizeof(PAYLOAD) - 1));
        editor.pump(2);
        const std::vector<std::string> replies = client.receive_all();
        usize ok = 0;
        usize failed = 0;
        for (const std::string &line : replies) {
            const core::result<protocol_response> response = parse_response(line);
            if (response.has_value()) {
                if (response.value().ok) {
                    ++ok;
                } else {
                    ++failed;
                    CHECK(response.value().errorCode == protocol_error_code::PARSE_ERROR);
                }
            }
        }
        CHECK(ok == 2);
        CHECK(failed == 1);
        CHECK(client.is_connected());
    }
}
