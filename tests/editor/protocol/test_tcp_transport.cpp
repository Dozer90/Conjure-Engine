#include "conjure/testing/doctest.h"

#include "conjure/testing/test_environment.h"

#include "editor_test_support.h"

#include "conjure/editor/protocol/protocol_message.h"
#include "editor/core/net/socket.h"
#include "editor/core/protocol/tcp_transport.h"

#include <array>
#include <string>
#include <vector>

// tcp_transport.h (private): the 127.0.0.1 transport, driven with raw loopback sockets from this side.

using namespace conjure;
using namespace conjure::editor;

namespace {
    struct polled {
        std::vector<u32> opened = {};
        std::vector<inbound_line> lines = {};
        std::vector<u32> closed = {};
    };

    /// <summary>
    /// Polls up to 'attempts' times until the predicate holds (the sockets are non-blocking on both ends).
    /// </summary>
    template <typename Predicate>
    polled poll_until(tcp_transport &transport, Predicate &&predicate, u32 attempts = 2000) {
        polled all = {};
        for (u32 attempt = 0; attempt < attempts; ++attempt) {
            polled step = {};
            transport.poll(step.opened, step.lines, step.closed);
            all.opened.insert(all.opened.end(), step.opened.begin(), step.opened.end());
            all.lines.insert(all.lines.end(), step.lines.begin(), step.lines.end());
            all.closed.insert(all.closed.end(), step.closed.begin(), step.closed.end());
            if (predicate(all)) {
                break;
            }
        }
        return all;
    }

    void send_all(const net::socket_handle &socket, std::string_view text) {
        usize offset = 0;
        u32 spins = 0;
        while (offset < text.size() && spins < 100000) {
            usize sent = 0;
            const std::span<const std::byte> bytes(
              reinterpret_cast<const std::byte *>(text.data()) + offset,
              text.size() - offset
            );
            const net::io_status status = net::send(socket, bytes, sent);
            REQUIRE(status != net::io_status::FAILED);
            REQUIRE(status != net::io_status::CLOSED);
            offset += sent;
            ++spins;
        }
        REQUIRE(offset == text.size());
    }

    std::string receive_some(const net::socket_handle &socket, usize atLeast, u32 attempts = 200000) {
        std::string received = {};
        std::array<std::byte, 4096> buffer = {};
        for (u32 attempt = 0; attempt < attempts && received.size() < atLeast; ++attempt) {
            usize count = 0;
            const net::io_status status = net::receive(socket, buffer, count);
            if (status == net::io_status::OK) {
                received.append(reinterpret_cast<const char *>(buffer.data()), count);
            } else if (status == net::io_status::CLOSED || status == net::io_status::FAILED) {
                break;
            }
        }
        return received;
    }
}

TEST_SUITE("editor/protocol/tcp_transport") {

    TEST_CASE("tcp_transport - binds an ephemeral port and reports it") {
        CONJURE_TEST_REQUIRE(testing::precondition::LOOPBACK_TCP);
        tcp_transport transport(0, 0);
        CONJURE_REQUIRE_OK(transport.start());
        CHECK(transport.bound_port() != 0);
        CHECK(transport.kind() == transport_kind::TCP);
        CHECK(transport.client_count() == 0);
        transport.stop();
    }

    TEST_CASE("tcp_transport - two requests in one packet and one request across two packets") {
        CONJURE_TEST_REQUIRE(testing::precondition::LOOPBACK_TCP);
        tcp_transport transport(0, 0);
        CONJURE_REQUIRE_OK(transport.start());
        CONJURE_REQUIRE_OK(net::initialise());
        const core::result<net::socket_handle> client = net::connect_loopback(transport.bound_port());
        CONJURE_REQUIRE_OK(client);
        polled opened = poll_until(transport, [](const polled &state) {
            return !state.opened.empty();
        });
        REQUIRE(opened.opened.size() == 1);
        const u32 clientId = opened.opened[0];
        send_all(client.value(), "{\"id\":1,\"cmd\":\"a\"}\n{\"id\":2,\"cmd\":\"b\"}\n");
        polled lines = poll_until(transport, [](const polled &state) {
            return state.lines.size() >= 2;
        });
        REQUIRE(lines.lines.size() == 2);
        CHECK(lines.lines[0].text == "{\"id\":1,\"cmd\":\"a\"}");
        CHECK(lines.lines[1].clientId == clientId);
        send_all(client.value(), "{\"id\":3,");
        (void) poll_until(transport, [](const polled &) {
            return true;
        }, 20);
        send_all(client.value(), "\"cmd\":\"c\"}\n");
        lines = poll_until(transport, [](const polled &state) {
            return !state.lines.empty();
        });
        REQUIRE(lines.lines.size() == 1);
        CHECK(lines.lines[0].text == "{\"id\":3,\"cmd\":\"c\"}");
        transport.send(clientId, "{\"id\":3,\"ok\":true}");
        transport.flush();
        const std::string reply = receive_some(client.value(), 20);
        CHECK(reply == "{\"id\":3,\"ok\":true}\n");
        transport.stop();
        net::shutdown_sockets();
    }

    TEST_CASE("tcp_transport - an oversized line is flagged and the client can be disconnected") {
        CONJURE_TEST_REQUIRE(testing::precondition::LOOPBACK_TCP);
        tcp_transport transport(0, 0, 64);
        CONJURE_REQUIRE_OK(transport.start());
        CONJURE_REQUIRE_OK(net::initialise());
        const core::result<net::socket_handle> client = net::connect_loopback(transport.bound_port());
        CONJURE_REQUIRE_OK(client);
        polled opened = poll_until(transport, [](const polled &state) {
            return !state.opened.empty();
        });
        REQUIRE(opened.opened.size() == 1);
        send_all(client.value(), std::string(200, 'x') + "\n");
        const polled lines = poll_until(transport, [](const polled &state) {
            return !state.lines.empty();
        });
        REQUIRE(lines.lines.size() == 1);
        CHECK(lines.lines[0].oversized);
        transport.send(opened.opened[0], "bye");
        transport.disconnect(opened.opened[0]);
        const polled closed = poll_until(transport, [](const polled &state) {
            return !state.closed.empty();
        });
        REQUIRE(closed.closed.size() == 1);
        CHECK(closed.closed[0] == opened.opened[0]);
        CHECK(transport.client_count() == 0);
        const std::string tail = receive_some(client.value(), 4);
        CHECK(tail == "bye\n");
        transport.stop();
        net::shutdown_sockets();
    }

    TEST_CASE("tcp_transport - a client that closes its socket is reported closed") {
        CONJURE_TEST_REQUIRE(testing::precondition::LOOPBACK_TCP);
        tcp_transport transport(0, 0);
        CONJURE_REQUIRE_OK(transport.start());
        CONJURE_REQUIRE_OK(net::initialise());
        {
            const core::result<net::socket_handle> client = net::connect_loopback(transport.bound_port());
            CONJURE_REQUIRE_OK(client);
            const polled opened = poll_until(transport, [](const polled &state) {
                return !state.opened.empty();
            });
            REQUIRE(opened.opened.size() == 1);
        }
        const polled closed = poll_until(transport, [](const polled &state) {
            return !state.closed.empty();
        });
        REQUIRE(closed.closed.size() == 1);
        CHECK(transport.client_count() == 0);
        transport.stop();
        net::shutdown_sockets();
    }

    TEST_CASE("tcp_transport - a busy explicit port fails without fallbacks and succeeds with them") {
        CONJURE_TEST_REQUIRE(testing::precondition::LOOPBACK_TCP);
        tcp_transport first(0, 0);
        CONJURE_REQUIRE_OK(first.start());
        const u16 busy = first.bound_port();
        tcp_transport second(busy, 0);
        const core::result<void> refused = second.start();
        CHECK_FALSE(refused.has_value());
        tcp_transport third(busy, 3);
        CONJURE_REQUIRE_OK(third.start());
        CHECK(third.bound_port() != busy);
        CHECK(third.bound_port() > busy);
        CHECK(third.bound_port() <= busy + 3);
        third.stop();
        first.stop();
    }

    TEST_CASE("tcp_transport - at most MAX_CLIENTS connections are accepted") {
        CONJURE_TEST_REQUIRE(testing::precondition::LOOPBACK_TCP);
        tcp_transport transport(0, 0);
        CONJURE_REQUIRE_OK(transport.start());
        CONJURE_REQUIRE_OK(net::initialise());
        std::vector<net::socket_handle> clients = {};
        for (usize index = 0; index < tcp_transport::MAX_CLIENTS + 2; ++index) {
            core::result<net::socket_handle> client = net::connect_loopback(transport.bound_port());
            if (client.has_value()) {
                clients.push_back(std::move(client).value());
            }
        }
        (void) poll_until(transport, [](const polled &state) {
            return state.opened.size() >= tcp_transport::MAX_CLIENTS;
        });
        (void) poll_until(transport, [](const polled &) {
            return true;
        }, 50);
        CHECK(transport.client_count() == tcp_transport::MAX_CLIENTS);
        transport.stop();
        net::shutdown_sockets();
    }
}
