#include "conjure/testing/doctest.h"

#include "conjure/editor/protocol/protocol_transport.h"
#include "editor/core/protocol/stdio_transport.h"

#include <sstream>
#include <string>
#include <vector>

// stdio_transport.h (private): the --ai-stdio transport over injected streams, read synchronously so no thread
// is involved.

using namespace conjure;
using namespace conjure::editor;

namespace {
    struct polled {
        std::vector<u32> opened = {};
        std::vector<inbound_line> lines = {};
        std::vector<u32> closed = {};
    };

    polled poll(stdio_transport &transport) {
        polled result = {};
        transport.poll(result.opened, result.lines, result.closed);
        return result;
    }
}

TEST_SUITE("editor/protocol/stdio_transport") {

    TEST_CASE("stdio_transport - opens one client, delivers the lines and reports the end of input") {
        std::istringstream input("{\"id\":1,\"cmd\":\"a\"}\n{\"id\":2,\"cmd\":\"b\"}\r\n");
        std::ostringstream output = {};
        stdio_transport transport(input, output, false);
        CHECK(transport.kind() == transport_kind::STDIO);
        CHECK(transport.client_count() == 0);
        CONJURE_REQUIRE_OK(transport.start());
        const polled result = poll(transport);
        REQUIRE(result.opened.size() == 1);
        CHECK(result.opened[0] == transport.client_id());
        REQUIRE(result.lines.size() == 2);
        CHECK(result.lines[0].text == "{\"id\":1,\"cmd\":\"a\"}");
        CHECK(result.lines[1].text == "{\"id\":2,\"cmd\":\"b\"}");
        CHECK(result.lines[1].clientId == transport.client_id());
        // The stream hit EOF: the client closes one poll later, after the responses to the last lines went out.
        CHECK(result.closed.empty());
        CHECK(transport.client_count() == 1);
        const polled again = poll(transport);
        REQUIRE(again.closed.size() == 1);
        CHECK(again.closed[0] == transport.client_id());
        CHECK(transport.client_count() == 0);
        const polled third = poll(transport);
        CHECK(third.closed.empty());
        transport.stop();
    }

    TEST_CASE("stdio_transport - responses are written as lines to the output stream") {
        std::istringstream input("{\"id\":1,\"cmd\":\"a\"}\n");
        std::ostringstream output = {};
        stdio_transport transport(input, output, false);
        CONJURE_REQUIRE_OK(transport.start());
        (void) poll(transport);
        transport.send(transport.client_id(), "{\"id\":1,\"ok\":true}");
        transport.send(transport.client_id() + 1, "ignored: wrong client");
        transport.flush();
        CHECK(output.str() == "{\"id\":1,\"ok\":true}\n");
        CHECK(transport.pending_bytes(transport.client_id()) == 0);
        transport.stop();
    }

    TEST_CASE("stdio_transport - an oversized line is flagged and a disconnect stops output") {
        std::istringstream input(std::string(32, 'x') + "\nok\n");
        std::ostringstream output = {};
        stdio_transport transport(input, output, false, 16);
        CONJURE_REQUIRE_OK(transport.start());
        const polled result = poll(transport);
        REQUIRE(result.lines.size() == 2);
        CHECK(result.lines[0].oversized);
        CHECK(result.lines[1].text == "ok");
        transport.disconnect(transport.client_id());
        transport.send(transport.client_id(), "dropped");
        CHECK(output.str().empty());
        const polled after = poll(transport);
        REQUIRE(after.closed.size() == 1);
        CHECK(after.closed[0] == transport.client_id());
        const polled later = poll(transport);
        CHECK(later.closed.empty());
        transport.stop();
    }

    TEST_CASE("regression - a line longer than the read chunk is discarded as it streams in, not accumulated") {
        // 200 KiB without a newline spans several READ_CHUNK_BYTES reads; with a 16-byte limit it is one
        // oversized marker, the lines around it arrive intact and a final unterminated line still counts.
        std::istringstream input(
          "first\n" + std::string(200 * 1024, 'x') + "\nsecond\n" + std::string(100 * 1024, 'y') + "\nlast"
        );
        std::ostringstream output = {};
        stdio_transport transport(input, output, false, 16);
        CHECK(stdio_transport::READ_CHUNK_BYTES < 200 * 1024);
        CONJURE_REQUIRE_OK(transport.start());
        const polled result = poll(transport);
        REQUIRE(result.lines.size() == 5);
        CHECK(result.lines[0].text == "first");
        CHECK(result.lines[1].oversized);
        CHECK(result.lines[2].text == "second");
        CHECK(result.lines[3].oversized);
        CHECK(result.lines[4].text == "last");
        CHECK_FALSE(result.lines[4].oversized);
        for (const inbound_line &line : result.lines) {
            CHECK(line.clientId == transport.client_id());
        }
        transport.stop();
    }

    TEST_CASE("stdio_transport - a line exactly one chunk long is reassembled across reads") {
        const std::string exact(stdio_transport::READ_CHUNK_BYTES - 1, 'e');
        const std::string longer(stdio_transport::READ_CHUNK_BYTES + 10, 'l');
        std::istringstream input(exact + "\n" + longer + "\nend\n");
        std::ostringstream output = {};
        stdio_transport transport(input, output, false);
        CONJURE_REQUIRE_OK(transport.start());
        const polled result = poll(transport);
        REQUIRE(result.lines.size() == 3);
        CHECK(result.lines[0].text == exact);
        CHECK(result.lines[1].text == longer);
        CHECK(result.lines[2].text == "end");
        transport.stop();
    }

    TEST_CASE("stdio_transport - starting twice is harmless and stop is idempotent") {
        std::istringstream input = {};
        std::ostringstream output = {};
        stdio_transport transport(input, output, false);
        CONJURE_REQUIRE_OK(transport.start());
        CONJURE_REQUIRE_OK(transport.start());
        transport.stop();
        transport.stop();
        const polled result = poll(transport);
        CHECK(result.opened.empty());
    }
}
