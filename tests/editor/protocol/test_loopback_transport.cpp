#include "conjure/testing/doctest.h"

#include "conjure/editor/protocol/loopback_transport.h"

#include <string>
#include <vector>

// loopback_transport.h: the in-process transport the UI panel and the tests use.

using namespace conjure;
using namespace conjure::editor;

namespace {
    struct polled {
        std::vector<u32> opened = {};
        std::vector<inbound_line> lines = {};
        std::vector<u32> closed = {};
    };

    polled poll(loopback_transport &transport) {
        polled result = {};
        transport.poll(result.opened, result.lines, result.closed);
        return result;
    }
}

TEST_SUITE("editor/protocol/loopback_transport") {

    TEST_CASE("loopback_transport - connect is reported at the next poll with an allocated id") {
        loopback_transport transport{};
        CHECK(transport.kind() == transport_kind::LOOPBACK);
        loopback_client client = transport.connect();
        CHECK(client.client_id() == 0);
        CONJURE_REQUIRE_OK(transport.start());
        const polled first = poll(transport);
        REQUIRE(first.opened.size() == 1);
        CHECK(first.opened[0] == 1);
        CHECK(client.client_id() == 1);
        CHECK(client.is_connected());
        CHECK(transport.client_count() == 1);
        transport.stop();
        CHECK_FALSE(client.is_connected());
    }

    TEST_CASE("loopback_transport - lines sent by the client arrive split and responses come back") {
        loopback_transport transport{};
        CONJURE_REQUIRE_OK(transport.start());
        loopback_client client = transport.connect();
        (void) poll(transport);
        client.send("{\"id\":1}");
        client.send_raw("{\"id\":2}\n{\"id\":3}\n{\"id\"");
        polled result = poll(transport);
        REQUIRE(result.lines.size() == 3);
        CHECK(result.lines[0].text == "{\"id\":1}");
        CHECK(result.lines[2].text == "{\"id\":3}");
        CHECK(result.lines[0].clientId == client.client_id());
        client.send_raw(":4}\n");
        result = poll(transport);
        REQUIRE(result.lines.size() == 1);
        CHECK(result.lines[0].text == "{\"id\":4}");

        transport.send(client.client_id(), "reply-a");
        transport.send(client.client_id(), "reply-b");
        CHECK(transport.pending_bytes(client.client_id()) == 16);
        CHECK(client.pending() == 2);
        const std::optional<std::string> a = client.receive();
        REQUIRE(a.has_value());
        CHECK(*a == "reply-a");
        const std::vector<std::string> rest = client.receive_all();
        REQUIRE(rest.size() == 1);
        CHECK(rest[0] == "reply-b");
        CHECK_FALSE(client.receive().has_value());
        CHECK(transport.pending_bytes(client.client_id()) == 0);
        transport.flush();
        transport.stop();
    }

    TEST_CASE("loopback_transport - an oversized line is reported once and the rest still parses") {
        loopback_transport transport(16);
        CONJURE_REQUIRE_OK(transport.start());
        loopback_client client = transport.connect();
        (void) poll(transport);
        client.send(std::string(40, 'x'));
        client.send("small");
        const polled result = poll(transport);
        REQUIRE(result.lines.size() == 2);
        bool oversized = false;
        bool small = false;
        for (const inbound_line &line : result.lines) {
            oversized = oversized || line.oversized;
            small = small || line.text == "small";
        }
        CHECK(oversized);
        CHECK(small);
        transport.stop();
    }

    TEST_CASE("loopback_transport - a client disconnect and a server disconnect both close the client") {
        loopback_transport transport{};
        CONJURE_REQUIRE_OK(transport.start());
        loopback_client first = transport.connect();
        loopback_client second = transport.connect();
        (void) poll(transport);
        CHECK(transport.client_count() == 2);
        first.disconnect();
        polled result = poll(transport);
        REQUIRE(result.closed.size() == 1);
        CHECK(result.closed[0] == first.client_id());
        CHECK(transport.client_count() == 1);
        transport.disconnect(second.client_id());
        result = poll(transport);
        REQUIRE(result.closed.size() == 1);
        CHECK(result.closed[0] == second.client_id());
        CHECK_FALSE(second.is_connected());
        CHECK(transport.client_count() == 0);
        // Sending to a gone client is ignored.
        transport.send(second.client_id(), "nobody");
        CHECK(transport.pending_bytes(second.client_id()) == 0);
        transport.stop();
    }

    TEST_CASE("loopback_client - a default client is detached") {
        loopback_client client = {};
        CHECK_FALSE(client.is_connected());
        CHECK(client.client_id() == 0);
        CHECK(client.pending() == 0);
        client.send("ignored");
        CHECK_FALSE(client.receive().has_value());
        CHECK(client.receive_all().empty());
        client.disconnect();
    }
}
