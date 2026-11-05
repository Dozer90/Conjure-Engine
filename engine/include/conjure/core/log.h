#pragma once

#include "conjure/core/platform_detect.h"
#include "conjure/core/types.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

// Logging.
//
//   CONJURE_DECLARE_LOG_CATEGORY(LOG_RENDERER);                     // in <module>/log_category.h
//   CONJURE_DEFINE_LOG_CATEGORY(LOG_RENDERER, "renderer");          // in one .cpp
//
//   CONJURE_LOG_INFO(LOG_RENDERER, "Created device {} ({} MiB)", name, megabytes);
//
// Messages are formatted with std::format only when the category and global levels allow them.
// Sinks receive fully formatted messages; the built-in sinks are console, file and callback.
// TRACE and DEBUG calls compile out of Distribution builds.

namespace conjure::core {

    enum class log_level : u8 {
        TRACE = 0,
        DEBUG,
        INFO,
        WARN,
        ERR,        // "ERROR" is a Windows macro (wingdi.h); ERR keeps the enum usable everywhere
        FATAL,
        OFF
    };

    /// <summary>
    /// Returns the upper-case name of a level ("INFO").
    /// </summary>
    [[nodiscard]] std::string_view to_string(log_level level) noexcept;

    /// <summary>
    /// Parses a level name case-insensitively. Returns OFF for unknown names.
    /// </summary>
    [[nodiscard]] log_level parse_log_level(std::string_view name) noexcept;

    /// <summary>
    /// A named channel with its own minimum level. Instances are global and register themselves
    /// so tools can list and adjust categories at runtime.
    /// </summary>
    class log_category {
        private:
            const char *mName = "";
            std::atomic<log_level> mLevel = log_level::TRACE;   // read by every logging thread

        public:
            explicit log_category(const char *name, log_level level = log_level::TRACE);
            ~log_category();

            log_category(const log_category &) = delete;
            log_category &operator=(const log_category &) = delete;

        public:
            /// <summary>
            /// The display name used in log lines and for lookup by name.
            /// </summary>
            [[nodiscard]] const char *name() const noexcept {
                return mName;
            }

            /// <summary>
            /// Minimum level that passes this category's filter.
            /// </summary>
            [[nodiscard]] log_level level() const noexcept {
                return mLevel.load(std::memory_order_relaxed);
            }

            /// <summary>
            /// Changes the minimum level. Safe to call from any thread.
            /// </summary>
            void set_level(log_level level) noexcept {
                mLevel.store(level, std::memory_order_relaxed);
            }

            /// <summary>
            /// True when a message at 'level' passes this category's filter.
            /// </summary>
            [[nodiscard]] bool is_enabled(log_level level) const noexcept {
                return level >= mLevel.load(std::memory_order_relaxed);
            }
    };

    /// <summary>
    /// A formatted message delivered to sinks. Views are valid only during the write call.
    /// </summary>
    struct log_message {
        log_level level = log_level::INFO;
        std::string_view category = {};
        std::string_view text = {};
        std::chrono::system_clock::time_point timestamp = {};
        std::thread::id thread_id = {};
        const char *file = "";
        int line = 0;
    };

    /// <summary>
    /// Destination for log messages. The logger calls write() from any thread, possibly
    /// concurrently, and without holding its own lock, so a sink must synchronise itself and
    /// may safely log or add/remove sinks from inside write().
    /// </summary>
    class log_sink {
        public:
            log_sink();
            virtual ~log_sink();

            log_sink(const log_sink &) = delete;
            log_sink &operator=(const log_sink &) = delete;

        public:
            virtual void write(const log_message &message) = 0;

            virtual void flush() {}
    };

    /// <summary>
    /// Writes to stdout (below WARN) and stderr (WARN and above) with optional ANSI colours.
    /// </summary>
    class console_log_sink final : public log_sink {
        private:
            bool mUseColour = true;

        public:
            explicit console_log_sink(bool useColour = true);
            ~console_log_sink() override;

        public:
            void write(const log_message &message) override;
            void flush() override;
    };

    /// <summary>
    /// Appends to a file. The file is created (with parent directories) on construction.
    /// </summary>
    class file_log_sink final : public log_sink {
        private:
            struct state;
            std::unique_ptr<state> mState = nullptr;   // owns the stream and its mutex

        public:
            explicit file_log_sink(const std::filesystem::path &path, bool truncate = true);
            ~file_log_sink() override;

        public:
            /// <summary>
            /// False when the file could not be opened; messages are then dropped silently.
            /// </summary>
            [[nodiscard]] bool is_open() const noexcept;

            void write(const log_message &message) override;
            void flush() override;
    };

    /// <summary>
    /// Forwards every message to a callable. Used by the editor console and by tests. The
    /// callable is invoked from the logging thread and must be thread-safe.
    /// </summary>
    class callback_log_sink final : public log_sink {
        private:
            std::function<void(const log_message &)> mCallback = {};

        public:
            explicit callback_log_sink(std::function<void(const log_message &)> callback);
            ~callback_log_sink() override;

        public:
            void write(const log_message &message) override;
    };

    /// <summary>
    /// Formats a message the way the console and file sinks do:
    /// "HH:MM:SS.mmm [LEVEL] [category] text".
    /// </summary>
    [[nodiscard]] std::string format_log_line(const log_message &message);

    namespace log {
        /// <summary>
        /// Adds a sink. Sinks are shared so the caller may keep a reference for later removal.
        /// </summary>
        void add_sink(std::shared_ptr<log_sink> sink);

        /// <summary>
        /// Removes a previously added sink. Unknown sinks are ignored.
        /// </summary>
        void remove_sink(const std::shared_ptr<log_sink> &sink);

        /// <summary>
        /// Removes every sink.
        /// </summary>
        void clear_sinks();

        /// <summary>
        /// The console sink that exists until disabled. On by default so early messages are
        /// never lost; tests turn it off.
        /// </summary>
        void set_console_enabled(bool enabled);
        [[nodiscard]] bool console_enabled() noexcept;

        /// <summary>
        /// Messages below this level are dropped regardless of category.
        /// </summary>
        void set_global_level(log_level level) noexcept;
        [[nodiscard]] log_level global_level() noexcept;

        /// <summary>
        /// True when a message at 'level' for 'category' would reach the sinks.
        /// </summary>
        [[nodiscard]] bool is_enabled(const log_category &category, log_level level) noexcept;

        /// <summary>
        /// Delivers an already formatted message to every sink.
        /// </summary>
        void write(const log_category &category, log_level level, const char *file, int line, std::string_view text);

        /// <summary>
        /// Flushes every sink.
        /// </summary>
        void flush();

        /// <summary>
        /// Every registered category, in registration order.
        /// </summary>
        [[nodiscard]] std::vector<log_category *> categories();

        /// <summary>
        /// Finds a category by name, or nullptr.
        /// </summary>
        [[nodiscard]] log_category *find_category(std::string_view name);

        /// <summary>
        /// Sets a category's level by name. Returns false when the category does not exist.
        /// </summary>
        bool set_category_level(std::string_view name, log_level level);
    }

    /// <summary>
    /// Category used by core itself and by code that has no better home.
    /// </summary>
    extern log_category LOG_CORE;
}

#define CONJURE_DECLARE_LOG_CATEGORY(name) extern ::conjure::core::log_category name
#define CONJURE_DEFINE_LOG_CATEGORY(name, displayName) ::conjure::core::log_category name(displayName)

#define CONJURE_LOG(category, level, ...)                                                                   \
    do {                                                                                                    \
        if (::conjure::core::log::is_enabled(category, level)) {                                            \
            ::conjure::core::log::write(category, level, __FILE__, __LINE__, std::format(__VA_ARGS__));     \
        }                                                                                                   \
    } while (false)

#if CONJURE_DISTRIBUTION
    // Keep the arguments in an unevaluated operand so they stay type-checked and never unused.
    #define CONJURE_LOG_TRACE(category, ...) static_cast<void>(sizeof(std::format(__VA_ARGS__)))
    #define CONJURE_LOG_DEBUG(category, ...) static_cast<void>(sizeof(std::format(__VA_ARGS__)))
#else
    #define CONJURE_LOG_TRACE(category, ...) CONJURE_LOG(category, ::conjure::core::log_level::TRACE, __VA_ARGS__)
    #define CONJURE_LOG_DEBUG(category, ...) CONJURE_LOG(category, ::conjure::core::log_level::DEBUG, __VA_ARGS__)
#endif

#define CONJURE_LOG_INFO(category, ...) CONJURE_LOG(category, ::conjure::core::log_level::INFO, __VA_ARGS__)
#define CONJURE_LOG_WARN(category, ...) CONJURE_LOG(category, ::conjure::core::log_level::WARN, __VA_ARGS__)
#define CONJURE_LOG_ERROR(category, ...) CONJURE_LOG(category, ::conjure::core::log_level::ERR, __VA_ARGS__)
#define CONJURE_LOG_FATAL(category, ...) CONJURE_LOG(category, ::conjure::core::log_level::FATAL, __VA_ARGS__)
