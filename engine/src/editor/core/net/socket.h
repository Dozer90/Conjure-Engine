#pragma once

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

// The one platform-specific layer of the editor (docs/design/04_editor.md 3.12): non-blocking loopback TCP
// sockets. socket_win32.cpp and socket_posix.cpp implement these functions; nothing else in the module knows
// the operating system.

namespace conjure::editor::net {

    using native_socket = std::intptr_t;

    inline constexpr native_socket INVALID_NATIVE_SOCKET = -1;

    /// <summary>
    /// Outcome of one non-blocking read or write.
    /// </summary>
    enum class io_status : u8 {
        OK,             // bytes moved (outCount)
        WOULD_BLOCK,    // nothing to do now
        CLOSED,         // the peer closed the connection
        FAILED          // the socket is broken; close it
    };

    /// <summary>
    /// Closes the socket it owns. Move-only.
    /// </summary>
    class socket_handle {
        private:
            native_socket mHandle = INVALID_NATIVE_SOCKET;

        public:
            socket_handle() noexcept = default;

            explicit socket_handle(native_socket handle) noexcept :
                mHandle(handle) {}

            ~socket_handle();

            socket_handle(const socket_handle &) = delete;
            socket_handle &operator=(const socket_handle &) = delete;

            socket_handle(socket_handle &&other) noexcept :
                mHandle(other.mHandle) {
                other.mHandle = INVALID_NATIVE_SOCKET;
            }

            socket_handle &operator=(socket_handle &&other) noexcept {
                if (this != &other) {
                    close();
                    mHandle = other.mHandle;
                    other.mHandle = INVALID_NATIVE_SOCKET;
                }
                return *this;
            }

        public:
            [[nodiscard]] native_socket get() const noexcept {
                return mHandle;
            }

            [[nodiscard]] bool valid() const noexcept {
                return mHandle != INVALID_NATIVE_SOCKET;
            }

            /// <summary>
            /// Closes now; the handle is invalid afterwards.
            /// </summary>
            void close() noexcept;
    };

    /// <summary>
    /// Initialises the socket library (WSAStartup on Windows; a no-op elsewhere). Reference counted.
    /// </summary>
    [[nodiscard]] core::result<void> initialise();

    /// <summary>
    /// Releases one initialise() reference.
    /// </summary>
    void shutdown_sockets() noexcept;

    /// <summary>
    /// True when a native socket error code means the port is taken or reserved, so the fallback port search
    /// moves on: EADDRINUSE, and on Windows also WSAEACCES (a port in a Hyper-V or WSL excluded range, or one
    /// another process holds with SO_EXCLUSIVEADDRUSE).
    /// </summary>
    [[nodiscard]] bool is_port_unavailable_error(int nativeCode) noexcept;

    /// <summary>
    /// Binds a non-blocking listening socket to 127.0.0.1:port (0 = ephemeral) with a backlog of 8 and
    /// reports the bound port. ALREADY_EXISTS when the port is in use or reserved (is_port_unavailable_error),
    /// IO_FAILURE otherwise. On POSIX every socket this layer creates (listening, accepted, connected) is
    /// close-on-exec, so a tool the editor spawns (dotnet, cmake) never inherits it.
    /// </summary>
    [[nodiscard]] core::result<socket_handle> listen_loopback(u16 port, u16 &outBoundPort);

    /// <summary>
    /// Accepts one pending connection as a non-blocking socket. An invalid handle (no error) when nothing
    /// is pending.
    /// </summary>
    [[nodiscard]] core::result<socket_handle> accept(const socket_handle &listener);

    /// <summary>
    /// Non-blocking read into the buffer.
    /// </summary>
    [[nodiscard]] io_status receive(const socket_handle &socket, std::span<std::byte> buffer, usize &outCount) noexcept;

    /// <summary>
    /// Non-blocking write of as many bytes as the socket accepts.
    /// </summary>
    [[nodiscard]] io_status send(
      const socket_handle &socket,
      std::span<const std::byte> data,
      usize &outCount
    ) noexcept;

    /// <summary>
    /// Connects to 127.0.0.1:port (blocking connect, then the socket is made non-blocking). For tests.
    /// </summary>
    [[nodiscard]] core::result<socket_handle> connect_loopback(u16 port);

    /// <summary>
    /// The last socket error's text (platform specific).
    /// </summary>
    [[nodiscard]] std::string last_error_text();
}
