#include "conjure/testing/doctest.h"

#include "conjure/editor/protocol/protocol_transport.h"

#include <string>
#include <vector>

// protocol_transport.h: the transport interface, its kinds and the client id allocation shared by every
// implementation.

using namespace conjure;
using namespace conjure::editor;

namespace {
    /// <summary>
    /// A transport that only exercises the base class: ids come from allocate_client_id().
    /// </summary>
    class counting_transport final : public protocol_transport {
        private:
            std::vector<u32> mOpened = {};

        public:
            counting_transport() = default;
            ~counting_transport() override = default;

        public:
            core::result<void> start() override {
                return core::result<void>::success();
            }

            void stop() override {}

            void poll(
              std::vector<u32> &outOpened,
              std::vector<inbound_line> &outLines,
              std::vector<u32> &outClosed
            ) override {
                (void) outLines;
                (void) outClosed;
                const u32 id = allocate_client_id();
                mOpened.push_back(id);
                outOpened.push_back(id);
            }

            void send(u32 clientId, std::string_view line) override {
                (void) clientId;
                (void) line;
            }

            void disconnect(u32 clientId) override {
                (void) clientId;
            }

            void flush() override {}

            usize pending_bytes(u32 clientId) const noexcept override {
                (void) clientId;
                return 0;
            }

            transport_kind kind() const noexcept override {
                return transport_kind::LOCAL;
            }

            usize client_count() const noexcept override {
                return mOpened.size();
            }
    };
}

TEST_SUITE("editor/protocol/protocol_transport") {

    TEST_CASE("transport_kind - names") {
        CHECK(to_string(transport_kind::LOCAL) == "local");
        CHECK(to_string(transport_kind::TCP) == "tcp");
        CHECK(to_string(transport_kind::STDIO) == "stdio");
        CHECK(to_string(transport_kind::LOOPBACK) == "loopback");
    }

    TEST_CASE("protocol_transport - without an allocator ids count up from one") {
        counting_transport transport = {};
        std::vector<u32> opened = {};
        std::vector<inbound_line> lines = {};
        std::vector<u32> closed = {};
        transport.poll(opened, lines, closed);
        transport.poll(opened, lines, closed);
        REQUIRE(opened.size() == 2);
        CHECK(opened[0] == 1);
        CHECK(opened[1] == 2);
        CHECK(transport.client_count() == 2);
    }

    TEST_CASE("protocol_transport - an installed allocator supplies the ids") {
        counting_transport transport = {};
        u32 next = 100;
        transport.set_client_id_allocator([&next]() {
            return next++;
        });
        std::vector<u32> opened = {};
        std::vector<inbound_line> lines = {};
        std::vector<u32> closed = {};
        transport.poll(opened, lines, closed);
        REQUIRE(opened.size() == 1);
        CHECK(opened[0] == 100);
        CHECK(next == 101);
    }

    TEST_CASE("inbound_line - defaults") {
        const inbound_line line = {};
        CHECK(line.text.empty());
        CHECK(line.clientId == 0);
        CHECK_FALSE(line.oversized);
    }
}
