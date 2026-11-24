#include "editor/core/protocol/tcp_transport.h"

#include "conjure/editor/editor_error.h"
#include "conjure/editor/log_category.h"
#include "conjure/editor/protocol/protocol_message.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <utility>

namespace conjure::editor {

    namespace {
        /// <summary>
        /// Tells a client beyond MAX_CLIENTS why it is about to be closed: one BUSY line with id null, sent once
        /// and best effort. The client has not been read from, so the close that follows is graceful and the
        /// line arrives before the end of the stream; a client that already sent bytes may see a reset instead.
        /// </summary>
        void refuse_busy(const net::socket_handle &socket) {
            const std::string refusal = serialise(make_error_response(
              null_id().root(),
              protocol_error_code::BUSY,
              std::format("the editor serves at most {} clients", tcp_transport::MAX_CLIENTS)
            )) + "\n";
            usize sent = 0;
            const std::span<const std::byte> bytes(reinterpret_cast<const std::byte *>(refusal.data()), refusal.size());
            (void) net::send(socket, bytes, sent);
        }
    }

    tcp_transport::tcp_transport(u16 port, u16 fallbackPorts, usize lineLimit) :
        mRequestedPort(port),
        mFallbackPorts(fallbackPorts),
        mLineLimit(lineLimit == 0 ? MAX_MESSAGE_BYTES : lineLimit) {
        mReceiveBuffer.resize(RECEIVE_CHUNK_BYTES);
    }

    tcp_transport::~tcp_transport() {
        stop();
    }

    core::result<void> tcp_transport::start() {
        if (mStarted) {
            return core::result<void>::success();
        }
        if (!mSocketsInitialised) {
            CONJURE_TRY(net::initialise());
            mSocketsInitialised = true;
        }
        core::error lastError = {};
        for (u32 attempt = 0; attempt <= mFallbackPorts; ++attempt) {
            const u32 candidate = static_cast<u32>(mRequestedPort) + attempt;
            if (candidate > 0xFFFFu) {
                break;
            }
            u16 bound = 0;
            core::result<net::socket_handle> listener = net::listen_loopback(static_cast<u16>(candidate), bound);
            if (listener.has_value()) {
                mListener = std::move(listener).value();
                mBoundPort = bound;
                mStarted = true;
                if (attempt > 0) {
                    CONJURE_LOG_INFO(
                      LOG_EDITOR,
                      "ai protocol: port {} was busy, listening on {}",
                      mRequestedPort,
                      bound
                    );
                }
                return core::result<void>::success();
            }
            lastError = listener.get_error();
            if (lastError.generic_code() != core::error_code::ALREADY_EXISTS) {
                break;
            }
            if (mRequestedPort == 0) {
                break;
            }
        }
        return core::make_error(
          protocol_error_code::IO_ERROR,
          std::format("ai protocol: cannot listen on 127.0.0.1:{}: {}", mRequestedPort, lastError.message())
        );
    }

    void tcp_transport::stop() {
        mConnections.clear();
        mPendingClosed.clear();
        mListener.close();
        mStarted = false;
        mBoundPort = 0;
        if (mSocketsInitialised) {
            net::shutdown_sockets();
            mSocketsInitialised = false;
        }
    }

    void tcp_transport::poll(
      std::vector<u32> &outOpened,
      std::vector<inbound_line> &outLines,
      std::vector<u32> &outClosed
    ) {
        if (!mStarted) {
            return;
        }
        accept_pending(outOpened);
        receive_all(outLines);
        for (usize index = 0; index < mConnections.size();) {
            connection &entry = mConnections[index];
            if (entry.closing && entry.outbound.empty()) {
                outClosed.push_back(entry.id);
                mConnections.erase(mConnections.begin() + static_cast<isize>(index));
                continue;
            }
            ++index;
        }
        for (const u32 id : mPendingClosed) {
            outClosed.push_back(id);
        }
        mPendingClosed.clear();
    }

    void tcp_transport::send(u32 clientId, std::string_view line) {
        connection *entry = find(clientId);
        if (entry == nullptr || entry->closing) {
            return;
        }
        entry->outbound.append(line);
        entry->outbound.push_back('\n');
    }

    void tcp_transport::disconnect(u32 clientId) {
        connection *entry = find(clientId);
        if (entry == nullptr) {
            return;
        }
        // Give the final response a chance to leave, then close at the next poll.
        flush_connection(*entry);
        entry->outbound.clear();
        entry->closing = true;
    }

    void tcp_transport::flush() {
        for (connection &entry : mConnections) {
            flush_connection(entry);
        }
    }

    usize tcp_transport::pending_bytes(u32 clientId) const noexcept {
        const connection *entry = find(clientId);
        return entry == nullptr ? 0 : entry->outbound.size();
    }

    transport_kind tcp_transport::kind() const noexcept {
        return transport_kind::TCP;
    }

    usize tcp_transport::client_count() const noexcept {
        usize count = 0;
        for (const connection &entry : mConnections) {
            if (!entry.closing) {
                ++count;
            }
        }
        return count;
    }

    tcp_transport::connection *tcp_transport::find(u32 clientId) noexcept {
        for (connection &entry : mConnections) {
            if (entry.id == clientId) {
                return &entry;
            }
        }
        return nullptr;
    }

    const tcp_transport::connection *tcp_transport::find(u32 clientId) const noexcept {
        for (const connection &entry : mConnections) {
            if (entry.id == clientId) {
                return &entry;
            }
        }
        return nullptr;
    }

    void tcp_transport::accept_pending(std::vector<u32> &outOpened) {
        for (u32 iteration = 0; iteration < 16; ++iteration) {
            core::result<net::socket_handle> accepted = net::accept(mListener);
            if (!accepted.has_value()) {
                CONJURE_LOG_WARN(LOG_EDITOR, "ai protocol: accept failed: {}", accepted.get_error().message());
                return;
            }
            if (!accepted.value().is_valid()) {
                return;
            }
            // Connections that are closing but still draining their last response do not hold a seat.
            if (client_count() >= MAX_CLIENTS) {
                CONJURE_LOG_WARN(LOG_EDITOR, "ai protocol: refusing a client beyond the limit of {}", MAX_CLIENTS);
                refuse_busy(accepted.value());
                continue;   // the handle closes here
            }
            connection entry = {};
            entry.socket = std::move(accepted).value();
            entry.splitter = line_splitter(mLineLimit);
            entry.id = allocate_client_id();
            outOpened.push_back(entry.id);
            mConnections.push_back(std::move(entry));
        }
    }

    void tcp_transport::receive_all(std::vector<inbound_line> &outLines) {
        for (connection &entry : mConnections) {
            if (entry.closing) {
                continue;
            }
            for (u32 iteration = 0; iteration < 64; ++iteration) {
                usize received = 0;
                const net::io_status status = net::receive(entry.socket, mReceiveBuffer, received);
                if (status == net::io_status::OK) {
                    entry.splitter.feed(std::span<const std::byte>(mReceiveBuffer.data(), received));
                    continue;
                }
                if (status == net::io_status::CLOSED || status == net::io_status::FAILED) {
                    entry.closing = true;
                    entry.outbound.clear();
                }
                break;
            }
            take_frames(entry.splitter, entry.id, outLines);
        }
    }

    void tcp_transport::take_frames(line_splitter &splitter, u32 clientId, std::vector<inbound_line> &outLines) {
        // Frames come out in arrival order, so an oversized marker sits exactly where the bad line was and the
        // server stops reading the client's lines at that point.
        std::string line = {};
        for (
          line_splitter::frame kind = splitter.next_frame(line);
          kind != line_splitter::frame::NONE;
          kind = splitter.next_frame(line)
        ) {
            outLines.push_back(inbound_line{std::move(line), clientId, kind == line_splitter::frame::OVERSIZED});
            line = {};
        }
    }

    void tcp_transport::flush_connection(connection &entry) {
        while (!entry.outbound.empty()) {
            usize sent = 0;
            const std::span<const std::byte> bytes(
              reinterpret_cast<const std::byte *>(entry.outbound.data()),
              entry.outbound.size()
            );
            const net::io_status status = net::send(entry.socket, bytes, sent);
            if (status == net::io_status::OK && sent > 0) {
                entry.outbound.erase(0, sent);
                continue;
            }
            if (status == net::io_status::CLOSED || status == net::io_status::FAILED) {
                entry.outbound.clear();
                entry.closing = true;
            }
            break;
        }
    }
}
