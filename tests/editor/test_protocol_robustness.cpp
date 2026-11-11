#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/editor/protocol/loopback_transport.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/editor/protocol/protocol_server.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// Protocol robustness (docs/design/08_testing.md 6.3 item 6): the corpus under tests/editor/data/protocol_corpus/
// (truncated JSON, deep nesting, huge numbers, invalid UTF-8, null bytes, duplicate keys, a 1 MiB string, empty
// lines, ...) never crashes the server; every entry yields an error response, a valid response, or nothing.

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
}

TEST_SUITE("editor/protocol_robustness") {

    TEST_CASE("protocol_robustness - the corpus has at least sixty entries") {
        CHECK(corpus_files().size() >= 60);
    }

    TEST_CASE("protocol_robustness - every corpus entry is answered or ignored, never fatal") {
        editor_test::headless_editor editor = {};
        editor.log().allow_errors();
        protocol_server &server = editor.application().server();
        std::unique_ptr<loopback_transport> loopback = std::make_unique<loopback_transport>();
        loopback_transport *transport = loopback.get();
        CONJURE_REQUIRE_OK(server.add_transport(std::move(loopback)));
        for (const std::filesystem::path &file : corpus_files()) {
            INFO("corpus entry ", file.filename().generic_string());
            loopback_client client = transport->connect();
            editor.pump(1);
            REQUIRE(client.is_connected());
            const std::string payload = editor_test::read_text_file(file);
            client.send_raw(payload);
            client.send_raw("\n{\"id\":\"probe\",\"cmd\":\"session.ping\"}\n");
            editor.pump(3);
            const std::vector<std::string> replies = client.receive_all();
            bool probeAnswered = false;
            for (const std::string &line : replies) {
                const core::result<protocol_response> response = parse_response(line);
                const core::result<protocol_event> event = parse_event(line);
                const bool parsed = response.has_value() || event.has_value();
                CHECK_MESSAGE(parsed, line.substr(0, 200));
                if (response.has_value() && response.value().id.root().as_string("") == "probe") {
                    probeAnswered = response.value().ok;
                }
            }
            // Either the probe got through (the entry was rejected or accepted line by line) or the entry was
            // oversized and the client was disconnected after a MESSAGE_TOO_LARGE answer.
            if (!probeAnswered) {
                CHECK_FALSE(client.is_connected());
                bool tooLarge = false;
                for (const std::string &line : replies) {
                    tooLarge = tooLarge || line.find("MESSAGE_TOO_LARGE") != std::string::npos;
                }
                CHECK(tooLarge);
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
