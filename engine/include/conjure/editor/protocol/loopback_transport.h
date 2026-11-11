#pragma once

#include "conjure/editor/protocol/protocol_transport.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The in-process transport (docs/design/04_editor.md 3.12): tests and the AI session panel connect a
// loopback_client, write lines into it and read the responses and events back, all on the main thread
// (a mutex still guards the queues so a test may drive a client from a job).

namespace conjure::editor {

    class loopback_transport;

    /// <summary>
    /// One connected in-process client.
    /// </summary>
    class CONJURE_API loopback_client {
        private:
            struct state;

            std::shared_ptr<state> mState = nullptr;

            friend class loopback_transport;

            explicit loopback_client(std::shared_ptr<state> shared) noexcept;

        public:
            loopback_client();
            ~loopback_client();

            loopback_client(const loopback_client &) = default;
            loopback_client &operator=(const loopback_client &) = default;

        public:
            /// <summary>
            /// Queues one line (a newline is appended when missing) for the server's next poll().
            /// </summary>
            void send(std::string_view line);

            /// <summary>
            /// Queues raw bytes without framing (partial lines, several lines in one chunk).
            /// </summary>
            void send_raw(std::string_view bytes);

            /// <summary>
            /// The next line the server sent, without its newline; nullopt when none is waiting.
            /// </summary>
            [[nodiscard]] std::optional<std::string> receive();

            /// <summary>
            /// Every waiting line.
            /// </summary>
            [[nodiscard]] std::vector<std::string> receive_all();

            /// <summary>
            /// Lines waiting to be received.
            /// </summary>
            [[nodiscard]] usize pending() const;

            /// <summary>
            /// Closes the client side; the server sees the disconnect at its next poll().
            /// </summary>
            void disconnect();

            [[nodiscard]] bool is_connected() const;

            /// <summary>
            /// The id the server assigned (0 until the server's first poll() accepted the client).
            /// </summary>
            [[nodiscard]] u32 client_id() const;
    };

    class CONJURE_API loopback_transport final : public protocol_transport {
        private:
            std::vector<std::shared_ptr<loopback_client::state>> mClients = {};
            std::vector<std::shared_ptr<loopback_client::state>> mPendingConnects = {};
            usize mLineLimit = 0;
            bool mStarted = false;

        public:
            explicit loopback_transport(usize lineLimit = 0);    // 0 = MAX_MESSAGE_BYTES
            ~loopback_transport() override;

        public:
            /// <summary>
            /// Creates a client; the server accepts it at its next poll().
            /// </summary>
            [[nodiscard]] loopback_client connect();

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

        private:
            [[nodiscard]] std::shared_ptr<loopback_client::state> find(u32 clientId) const noexcept;
    };
}
