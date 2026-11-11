#pragma once

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// A transport (docs/design/04_editor.md 3.12) moves complete lines between clients and the protocol server
// on the main thread: poll() reports connects, lines and disconnects since the last call; send() queues a
// line; flush() writes what the peer accepts without blocking. Client ids come from the server through the
// allocator so every transport's clients share one id space.

namespace conjure::editor {

    enum class transport_kind : u8 {
        LOCAL = 0,          // the UI (client 0, no transport)
        TCP,
        STDIO,
        LOOPBACK
    };

    /// <summary>
    /// The wire name of a kind ("tcp").
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(transport_kind kind) noexcept;

    /// <summary>
    /// One complete line received from a client. 'oversized' marks a line that exceeded the limit (text is
    /// then empty): the server answers MESSAGE_TOO_LARGE and disconnects.
    /// </summary>
    struct inbound_line {
        std::string text = "";
        u32 clientId = 0;
        bool oversized = false;
    };

    /// <summary>
    /// Hands out client ids.
    /// </summary>
    using client_id_allocator = std::function<u32()>;

    class CONJURE_API protocol_transport {
        public:
            protocol_transport();
            virtual ~protocol_transport();

            protocol_transport(const protocol_transport &) = delete;
            protocol_transport &operator=(const protocol_transport &) = delete;

        public:
            /// <summary>
            /// Opens the transport (binds the socket, starts the reader thread).
            /// </summary>
            [[nodiscard]] virtual core::result<void> start() = 0;

            /// <summary>
            /// Closes every client and releases the resources. Idempotent.
            /// </summary>
            virtual void stop() = 0;

            /// <summary>
            /// Collects what happened since the last call: new clients (ids allocated through the allocator),
            /// complete lines and closed clients, in that order. Never blocks.
            /// </summary>
            virtual void poll(
              std::vector<u32> &outOpened,
              std::vector<inbound_line> &outLines,
              std::vector<u32> &outClosed
            ) = 0;

            /// <summary>
            /// Queues a line (a newline is appended) for a client. Unknown ids are ignored.
            /// </summary>
            virtual void send(u32 clientId, std::string_view line) = 0;

            /// <summary>
            /// Closes a client; the next poll() reports it closed.
            /// </summary>
            virtual void disconnect(u32 clientId) = 0;

            /// <summary>
            /// Writes queued bytes without blocking.
            /// </summary>
            virtual void flush() = 0;

            /// <summary>
            /// Bytes queued for a client and not yet written (the backlog policy reads it).
            /// </summary>
            [[nodiscard]] virtual usize pending_bytes(u32 clientId) const noexcept = 0;

            [[nodiscard]] virtual transport_kind kind() const noexcept = 0;

            /// <summary>
            /// Connected clients.
            /// </summary>
            [[nodiscard]] virtual usize client_count() const noexcept = 0;

            /// <summary>
            /// The server installs its id allocator before start(). Without one ids count from 1.
            /// </summary>
            void set_client_id_allocator(client_id_allocator allocator);

        protected:
            /// <summary>
            /// The next client id from the allocator (or a local counter).
            /// </summary>
            [[nodiscard]] u32 allocate_client_id();

        private:
            client_id_allocator mAllocator = {};
            u32 mNextLocalId = 1;
    };
}
