#pragma once

#include "conjure/core/api.h"

#include <functional>
#include <stop_token>
#include <string>
#include <thread>

namespace conjure::core::jobs {

    /// <summary>
    /// A named, long-running thread outside the worker pool (file watchers, process pipe readers,
    /// audio streaming). The body receives a stop token; destruction requests a stop and joins.
    /// Move-only.
    /// </summary>
    class CONJURE_API dedicated_thread {
        private:
            std::string mName = {};
            std::jthread mThread = {};

        public:
            /// <summary>
            /// Starts the thread immediately. The name is applied to the OS thread (truncated on a
            /// code point boundary to 15 bytes on Linux and macOS and 63 UTF-16 units on Windows). An
            /// empty body asserts and leaves the object non-joinable.
            /// </summary>
            dedicated_thread(std::string name, std::function<void(std::stop_token)> body);

            /// <summary>
            /// Requests a stop and joins when the thread is still running.
            /// </summary>
            ~dedicated_thread();

            dedicated_thread(const dedicated_thread &) = delete;
            dedicated_thread &operator=(const dedicated_thread &) = delete;
            dedicated_thread(dedicated_thread &&) noexcept = default;
            dedicated_thread &operator=(dedicated_thread &&) noexcept = default;

        public:
            /// <summary>
            /// Signals the body's stop token. Does not wait.
            /// </summary>
            void request_stop() noexcept;

            /// <summary>
            /// True once request_stop() has been called (or the destructor has started).
            /// </summary>
            [[nodiscard]] bool stop_requested() const noexcept;

            /// <summary>
            /// True while the thread has been started and not yet joined.
            /// </summary>
            [[nodiscard]] bool joinable() const noexcept;

            /// <summary>
            /// Waits for the body to return. Does not request a stop; no-op when not joinable.
            /// </summary>
            void join();

            /// <summary>
            /// The name given at construction.
            /// </summary>
            [[nodiscard]] const std::string &name() const noexcept;

            /// <summary>
            /// Identifier of the underlying thread; default-constructed when not joinable.
            /// </summary>
            [[nodiscard]] std::thread::id get_id() const noexcept;
    };
}
