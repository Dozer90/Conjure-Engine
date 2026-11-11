#include "editor/core/net/socket.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <format>
#include <string>

namespace conjure::editor::net {

    namespace {
        [[nodiscard]] core::error socket_error(std::string_view what, int code) {
            return core::error(
              code == EADDRINUSE ? core::error_code::ALREADY_EXISTS : core::error_code::IO_FAILURE,
              std::format("{}: {} ({})", what, std::strerror(code), code)
            );
        }

        [[nodiscard]] bool set_non_blocking(int descriptor) noexcept {
            const int flags = fcntl(descriptor, F_GETFL, 0);
            if (flags < 0) {
                return false;
            }
            return fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) == 0;
        }

        /// <summary>
        /// A peer that went away must not raise SIGPIPE on our next send: Linux passes MSG_NOSIGNAL per call,
        /// Apple's libc has no such flag and uses the SO_NOSIGPIPE socket option instead (a backend file, so
        /// the platform check is allowed here).
        /// </summary>
        void suppress_sigpipe(int descriptor) noexcept {
#if defined(SO_NOSIGPIPE)
            int enable = 1;
            setsockopt(descriptor, SOL_SOCKET, SO_NOSIGPIPE, &enable, sizeof(enable));
#else
            (void) descriptor;
#endif
        }

#if defined(MSG_NOSIGNAL)
        constexpr int SEND_FLAGS = MSG_NOSIGNAL;
#else
        constexpr int SEND_FLAGS = 0;
#endif
    }

    socket_handle::~socket_handle() {
        close();
    }

    void socket_handle::close() noexcept {
        if (mHandle != INVALID_NATIVE_SOCKET) {
            ::close(static_cast<int>(mHandle));
            mHandle = INVALID_NATIVE_SOCKET;
        }
    }

    core::result<void> initialise() {
        return core::result<void>::success();
    }

    void shutdown_sockets() noexcept {}

    core::result<socket_handle> listen_loopback(u16 port, u16 &outBoundPort) {
        const int raw = ::socket(AF_INET, SOCK_STREAM, 0);
        if (raw < 0) {
            return socket_error("socket", errno);
        }
        socket_handle handle(raw);
        int reuse = 1;
        setsockopt(raw, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        sockaddr_in address = {};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::bind(raw, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0) {
            return socket_error(std::format("bind 127.0.0.1:{}", port), errno);
        }
        if (::listen(raw, 8) != 0) {
            return socket_error("listen", errno);
        }
        if (!set_non_blocking(raw)) {
            return socket_error("fcntl(O_NONBLOCK)", errno);
        }
        sockaddr_in bound = {};
        socklen_t length = sizeof(bound);
        if (getsockname(raw, reinterpret_cast<sockaddr *>(&bound), &length) != 0) {
            return socket_error("getsockname", errno);
        }
        outBoundPort = ntohs(bound.sin_port);
        return handle;
    }

    core::result<socket_handle> accept(const socket_handle &listener) {
        const int raw = ::accept(static_cast<int>(listener.get()), nullptr, nullptr);
        if (raw < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return socket_handle();
            }
            return socket_error("accept", errno);
        }
        socket_handle handle(raw);
        suppress_sigpipe(raw);
        if (!set_non_blocking(raw)) {
            return socket_error("fcntl(O_NONBLOCK)", errno);
        }
        return handle;
    }

    io_status receive(const socket_handle &socket, std::span<std::byte> buffer, usize &outCount) noexcept {
        outCount = 0;
        if (buffer.empty()) {
            return io_status::WOULD_BLOCK;
        }
        const ssize_t received = ::recv(static_cast<int>(socket.get()), buffer.data(), buffer.size(), 0);
        if (received > 0) {
            outCount = static_cast<usize>(received);
            return io_status::OK;
        }
        if (received == 0) {
            return io_status::CLOSED;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return io_status::WOULD_BLOCK;
        }
        if (errno == ECONNRESET || errno == EPIPE) {
            return io_status::CLOSED;
        }
        return io_status::FAILED;
    }

    io_status send(const socket_handle &socket, std::span<const std::byte> data, usize &outCount) noexcept {
        outCount = 0;
        if (data.empty()) {
            return io_status::OK;
        }
        const ssize_t sent = ::send(static_cast<int>(socket.get()), data.data(), data.size(), SEND_FLAGS);
        if (sent >= 0) {
            outCount = static_cast<usize>(sent);
            return io_status::OK;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return io_status::WOULD_BLOCK;
        }
        if (errno == ECONNRESET || errno == EPIPE) {
            return io_status::CLOSED;
        }
        return io_status::FAILED;
    }

    core::result<socket_handle> connect_loopback(u16 port) {
        const int raw = ::socket(AF_INET, SOCK_STREAM, 0);
        if (raw < 0) {
            return socket_error("socket", errno);
        }
        socket_handle handle(raw);
        sockaddr_in address = {};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::connect(raw, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0) {
            return socket_error(std::format("connect 127.0.0.1:{}", port), errno);
        }
        suppress_sigpipe(raw);
        if (!set_non_blocking(raw)) {
            return socket_error("fcntl(O_NONBLOCK)", errno);
        }
        return handle;
    }

    std::string last_error_text() {
        return std::format("{} ({})", std::strerror(errno), errno);
    }
}
