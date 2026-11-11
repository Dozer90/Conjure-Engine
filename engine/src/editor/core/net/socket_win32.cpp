#include "editor/core/net/socket.h"

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
    #define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <format>
#include <string>

namespace conjure::editor::net {

    namespace {
        std::atomic<int> sInitialiseCount = 0;

        [[nodiscard]] core::error socket_error(std::string_view what, int code) {
            return core::error(code == WSAEADDRINUSE ? core::error_code::ALREADY_EXISTS : core::error_code::IO_FAILURE,
              std::format("{}: winsock error {}", what, code));
        }

        [[nodiscard]] bool set_non_blocking(SOCKET socket) noexcept {
            u_long mode = 1;
            return ioctlsocket(socket, FIONBIO, &mode) == 0;
        }
    }

    socket_handle::~socket_handle() {
        close();
    }

    void socket_handle::close() noexcept {
        if (mHandle != INVALID_NATIVE_SOCKET) {
            closesocket(static_cast<SOCKET>(mHandle));
            mHandle = INVALID_NATIVE_SOCKET;
        }
    }

    core::result<void> initialise() {
        if (sInitialiseCount.fetch_add(1) == 0) {
            WSADATA data = {};
            const int started = WSAStartup(MAKEWORD(2, 2), &data);
            if (started != 0) {
                sInitialiseCount.fetch_sub(1);
                return socket_error("WSAStartup", started);
            }
        }
        return core::result<void>::success();
    }

    void shutdown_sockets() noexcept {
        if (sInitialiseCount.load() > 0 && sInitialiseCount.fetch_sub(1) == 1) {
            WSACleanup();
        }
    }

    core::result<socket_handle> listen_loopback(u16 port, u16 &outBoundPort) {
        const SOCKET raw = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (raw == INVALID_SOCKET) {
            return socket_error("socket", WSAGetLastError());
        }
        socket_handle handle(static_cast<native_socket>(raw));
        // Exclusive bind so a busy port is reported instead of silently shared.
        int exclusive = 1;
        setsockopt(raw, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char *>(&exclusive), sizeof(exclusive));

        sockaddr_in address = {};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::bind(raw, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0) {
            return socket_error(std::format("bind 127.0.0.1:{}", port), WSAGetLastError());
        }
        if (::listen(raw, 8) != 0) {
            return socket_error("listen", WSAGetLastError());
        }
        if (!set_non_blocking(raw)) {
            return socket_error("ioctlsocket(FIONBIO)", WSAGetLastError());
        }
        sockaddr_in bound = {};
        int length = sizeof(bound);
        if (getsockname(raw, reinterpret_cast<sockaddr *>(&bound), &length) != 0) {
            return socket_error("getsockname", WSAGetLastError());
        }
        outBoundPort = ntohs(bound.sin_port);
        return handle;
    }

    core::result<socket_handle> accept(const socket_handle &listener) {
        const SOCKET raw = ::accept(static_cast<SOCKET>(listener.get()), nullptr, nullptr);
        if (raw == INVALID_SOCKET) {
            const int code = WSAGetLastError();
            if (code == WSAEWOULDBLOCK) {
                return socket_handle();
            }
            return socket_error("accept", code);
        }
        socket_handle handle(static_cast<native_socket>(raw));
        if (!set_non_blocking(raw)) {
            return socket_error("ioctlsocket(FIONBIO)", WSAGetLastError());
        }
        return handle;
    }

    io_status receive(const socket_handle &socket, std::span<std::byte> buffer, usize &outCount) noexcept {
        outCount = 0;
        if (buffer.empty()) {
            return io_status::WOULD_BLOCK;
        }
        const int received = ::recv(
          static_cast<SOCKET>(socket.get()),
          reinterpret_cast<char *>(buffer.data()),
          static_cast<int>(buffer.size() > 0x7FFFFFFF ? 0x7FFFFFFF : buffer.size()),
          0
        );
        if (received > 0) {
            outCount = static_cast<usize>(received);
            return io_status::OK;
        }
        if (received == 0) {
            return io_status::CLOSED;
        }
        const int code = WSAGetLastError();
        if (code == WSAEWOULDBLOCK) {
            return io_status::WOULD_BLOCK;
        }
        if (code == WSAECONNRESET || code == WSAECONNABORTED || code == WSAESHUTDOWN) {
            return io_status::CLOSED;
        }
        return io_status::FAILED;
    }

    io_status send(const socket_handle &socket, std::span<const std::byte> data, usize &outCount) noexcept {
        outCount = 0;
        if (data.empty()) {
            return io_status::OK;
        }
        const int sent = ::send(
          static_cast<SOCKET>(socket.get()),
          reinterpret_cast<const char *>(data.data()),
          static_cast<int>(data.size() > 0x7FFFFFFF ? 0x7FFFFFFF : data.size()),
          0
        );
        if (sent >= 0) {
            outCount = static_cast<usize>(sent);
            return io_status::OK;
        }
        const int code = WSAGetLastError();
        if (code == WSAEWOULDBLOCK) {
            return io_status::WOULD_BLOCK;
        }
        if (code == WSAECONNRESET || code == WSAECONNABORTED || code == WSAESHUTDOWN) {
            return io_status::CLOSED;
        }
        return io_status::FAILED;
    }

    core::result<socket_handle> connect_loopback(u16 port) {
        const SOCKET raw = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (raw == INVALID_SOCKET) {
            return socket_error("socket", WSAGetLastError());
        }
        socket_handle handle(static_cast<native_socket>(raw));
        sockaddr_in address = {};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::connect(raw, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0) {
            return socket_error(std::format("connect 127.0.0.1:{}", port), WSAGetLastError());
        }
        if (!set_non_blocking(raw)) {
            return socket_error("ioctlsocket(FIONBIO)", WSAGetLastError());
        }
        return handle;
    }

    std::string last_error_text() {
        return std::format("winsock error {}", WSAGetLastError());
    }
}
