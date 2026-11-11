#pragma once

#include "editor/core/net/socket.h"

#include "conjure/editor/protocol/line_splitter.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/editor/protocol/protocol_transport.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <string>
#include <vector>

// The TCP transport (docs/design/04_editor.md 3.12): 127.0.0.1 only, non-blocking accept, read and write
// polled from the main thread, up to MAX_CLIENTS connections, fallback ports when the first is busy and
// not explicit.

namespace conjure::editor {

    class tcp_transport final : public protocol_transport {
        public:
            static constexpr usize MAX_CLIENTS = 8;
            static constexpr usize RECEIVE_CHUNK_BYTES = 64 * 1024;

        private:
            struct connection {
                net::socket_handle socket = {};
                line_splitter splitter{};
                std::string outbound = "";
                u32 id = 0;
                bool closing = false;
            };

            net::socket_handle mListener = {};
            std::vector<connection> mConnections = {};
            std::vector<u32> mPendingClosed = {};
            std::vector<std::byte> mReceiveBuffer = {};
            u16 mRequestedPort = 0;
            u16 mFallbackPorts = 0;
            u16 mBoundPort = 0;
            usize mLineLimit = MAX_MESSAGE_BYTES;
            bool mStarted = false;
            bool mSocketsInitialised = false;

        public:
            /// <summary>
            /// Binds 'port' (0 = ephemeral) at start(); when busy, the next 'fallbackPorts' ports are tried.
            /// </summary>
            tcp_transport(u16 port, u16 fallbackPorts, usize lineLimit = MAX_MESSAGE_BYTES);
            ~tcp_transport() override;

        public:
            [[nodiscard]] core::result<void> start() override;
            void stop() override;
            void poll(
              std::vector<u32> &outOpened,
              std::vector<inbound_line> &outLines,
              std::vector<u32> &outClosed
            ) override;
            void send(u32 clientId, std::string_view line) override;
            void disconnect(u32 clientId) override;
            void flush() override;
            [[nodiscard]] usize pending_bytes(u32 clientId) const noexcept override;
            [[nodiscard]] transport_kind kind() const noexcept override;
            [[nodiscard]] usize client_count() const noexcept override;

            /// <summary>
            /// The port the listener bound (0 before start()).
            /// </summary>
            [[nodiscard]] u16 bound_port() const noexcept {
                return mBoundPort;
            }

        private:
            [[nodiscard]] connection *find(u32 clientId) noexcept;
            [[nodiscard]] const connection *find(u32 clientId) const noexcept;
            void accept_pending(std::vector<u32> &outOpened);
            void receive_all(std::vector<inbound_line> &outLines);
            void flush_connection(connection &entry);

            /// <summary>
            /// Moves every complete frame of the splitter into outLines in arrival order (markers included).
            /// </summary>
            static void take_frames(line_splitter &splitter, u32 clientId, std::vector<inbound_line> &outLines);
    };
}
