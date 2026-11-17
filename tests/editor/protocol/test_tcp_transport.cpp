#include "conjure/testing/doctest.h"

#include "conjure/testing/test_environment.h"

#include "editor_test_support.h"

#include "conjure/core/platform_detect.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "editor/core/net/socket.h"
#include "editor/core/protocol/tcp_transport.h"

#if !CONJURE_PLATFORM_WINDOWS
    #include <fcntl.h>
#endif

#include <cerrno>
#include <string>
#include <vector>

// tcp_transport.h (private): the 127.0.0.1 transport, driven with raw loopback sockets from this side.

using namespace conjure;
using namespace conjure::editor;
using editor_test::receive_some;
using editor_test::send_all;

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

    TEST_CASE("tcp_transport - a client beyond MAX_CLIENTS receives one BUSY line before the close") {
        CONJURE_TEST_REQUIRE(testing::precondition::LOOPBACK_TCP);
        tcp_transport transport(0, 0);
        CONJURE_REQUIRE_OK(transport.start());
        CONJURE_REQUIRE_OK(net::initialise());
        std::vector<net::socket_handle> clients = {};
        for (usize index = 0; index < tcp_transport::MAX_CLIENTS; ++index) {
            core::result<net::socket_handle> client = net::connect_loopback(transport.bound_port());
            CONJURE_REQUIRE_OK(client);
            clients.push_back(std::move(client).value());
        }
        const polled opened = poll_until(transport, [](const polled &state) {
            return state.opened.size() >= tcp_transport::MAX_CLIENTS;
        });
        REQUIRE(opened.opened.size() == tcp_transport::MAX_CLIENTS);

        const core::result<net::socket_handle> extra = net::connect_loopback(transport.bound_port());
        CONJURE_REQUIRE_OK(extra);
        const polled after = poll_until(transport, [](const polled &) {
            return false;
        }, 50);
        CHECK(after.opened.empty());
        CHECK(transport.client_count() == tcp_transport::MAX_CLIENTS);
        const std::string refusal = receive_some(extra.value(), 1024);
        CHECK(refusal.find(R"("code":"BUSY")") != std::string::npos);
        CHECK(refusal.find(R"("id":null)") != std::string::npos);
        REQUIRE_FALSE(refusal.empty());
        CHECK(refusal.back() == '\n');
        CHECK(refusal.find('\n') == refusal.size() - 1);
        transport.stop();
        net::shutdown_sockets();
    }

    TEST_CASE("socket - a reserved or busy port counts as unavailable for the fallback search") {
#if CONJURE_PLATFORM_WINDOWS
        // WSAEACCES: a port in an excluded range (Hyper-V, WSL) or held with SO_EXCLUSIVEADDRUSE.
        constexpr int RESERVED = 10013;
        constexpr int IN_USE = 10048;
        constexpr int REFUSED = 10061;
        CHECK(net::is_port_unavailable_error(RESERVED));
        CHECK(net::is_port_unavailable_error(IN_USE));
        CHECK_FALSE(net::is_port_unavailable_error(REFUSED));
#else
        CHECK(net::is_port_unavailable_error(EADDRINUSE));
        CHECK_FALSE(net::is_port_unavailable_error(ECONNREFUSED));
#endif
        CHECK_FALSE(net::is_port_unavailable_error(0));
    }

#if !CONJURE_PLATFORM_WINDOWS
    TEST_CASE("tcp_transport - listening, accepted and connected sockets are close-on-exec [posix]") {
        CONJURE_TEST_REQUIRE(testing::precondition::LOOPBACK_TCP);
        CONJURE_REQUIRE_OK(net::initialise());
        u16 bound = 0;
        const core::result<net::socket_handle> listener = net::listen_loopback(0, bound);
        CONJURE_REQUIRE_OK(listener);
        CHECK((fcntl(static_cast<int>(listener.value().get()), F_GETFD) & FD_CLOEXEC) != 0);
        const core::result<net::socket_handle> client = net::connect_loopback(bound);
        CONJURE_REQUIRE_OK(client);
        CHECK((fcntl(static_cast<int>(client.value().get()), F_GETFD) & FD_CLOEXEC) != 0);
        core::result<net::socket_handle> accepted = net::accept(listener.value());
        for (u32 attempt = 0; attempt < 200000 && accepted.has_value() && !accepted.value().valid(); ++attempt) {
            accepted = net::accept(listener.value());
        }
        CONJURE_REQUIRE_OK(accepted);
        REQUIRE(accepted.value().valid());
        CHECK((fcntl(static_cast<int>(accepted.value().get()), F_GETFD) & FD_CLOEXEC) != 0);
        net::shutdown_sockets();
    }
#endif
}
