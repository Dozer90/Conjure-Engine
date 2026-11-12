#include "conjure/core/log.h"

#include "core/foundation/console_output.h"
#include "core/foundation/local_time.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <mutex>

using namespace conjure::core;

/*===========================================================================*/
/*===========================================================================*/
// Levels

namespace {
    constexpr std::array<std::string_view, 7> LEVEL_NAMES = {
        "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL", "OFF",
    };
}

std::string_view conjure::core::to_string(log_level level) noexcept {
    const conjure::usize index = static_cast<conjure::usize>(level);
    if (index >= LEVEL_NAMES.size()) {
        return "UNKNOWN";
    }
    return LEVEL_NAMES[index];
}

log_level conjure::core::parse_log_level(std::string_view name) noexcept {
    for (conjure::usize index = 0; index < LEVEL_NAMES.size(); ++index) {
        const std::string_view candidate = LEVEL_NAMES[index];
        if (candidate.size() != name.size()) {
            continue;
        }

        bool matches = true;
        for (conjure::usize character = 0; character < name.size(); ++character) {
            const int lhs = std::toupper(static_cast<unsigned char>(name[character]));
            const int rhs = static_cast<unsigned char>(candidate[character]);
            if (lhs != rhs) {
                matches = false;
                break;
            }
        }

        if (matches) {
            return static_cast<log_level>(index);
        }
    }
    return log_level::OFF;
}


/*===========================================================================*/
/*===========================================================================*/
// Logger state

namespace {
    struct logger_state {
        std::mutex mutex = {};
        std::vector<std::shared_ptr<log_sink>> sinks = {};
        std::vector<log_category *> categories = {};
        std::shared_ptr<console_log_sink> console = nullptr;
        std::atomic<bool> consoleEnabled = true;
        std::atomic<log_level> globalLevel = log_level::TRACE;
    };

    // Function-local static so the logger is usable from the constructors of global categories,
    // whatever translation unit they live in.
    logger_state &state() {
        static logger_state instance = {};
        return instance;
    }

    struct sink_snapshot {
        std::shared_ptr<console_log_sink> console = nullptr;
        std::vector<std::shared_ptr<log_sink>> sinks = {};
    };

    // Copies the sink list under the lock so sinks run unlocked and may log or change the sink
    // list themselves without deadlocking.
    sink_snapshot snapshot_sinks(logger_state &logger) {
        sink_snapshot snapshot = {};
        const std::scoped_lock lock(logger.mutex);
        if (logger.consoleEnabled.load(std::memory_order_relaxed)) {
            if (!logger.console) {
                logger.console = std::make_shared<console_log_sink>();
            }
            snapshot.console = logger.console;
        }
        snapshot.sinks = logger.sinks;
        return snapshot;
    }
}


/*===========================================================================*/
/*===========================================================================*/
// log_category

log_category::log_category(const char *name, log_level level) :
    mName(name),
    mLevel(level) {
    logger_state &logger = state();
    const std::scoped_lock lock(logger.mutex);
    logger.categories.push_back(this);
}

log_category::~log_category() {
    logger_state &logger = state();
    const std::scoped_lock lock(logger.mutex);
    std::erase(logger.categories, this);
}

log_category conjure::core::LOG_CORE("core");


/*===========================================================================*/
/*===========================================================================*/
// Formatting

std::string conjure::core::format_log_line(const log_message &message) {
    // Local wall-clock time without std::chrono time zones, which Apple's libc++ lacks.
    const std::tm local = detail::local_time_of(std::chrono::system_clock::to_time_t(message.timestamp));
    const auto sinceEpoch = message.timestamp.time_since_epoch();
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(sinceEpoch).count() % 1000;

    return std::format(
      "{:02}:{:02}:{:02}.{:03} [{:5}] [{}] {}",
      local.tm_hour,
      local.tm_min,
      local.tm_sec,
      milliseconds,
      to_string(message.level),
      message.category,
      message.text
    );
}


/*===========================================================================*/
/*===========================================================================*/
// log_sink

log_sink::log_sink() {}

log_sink::~log_sink() {}


/*===========================================================================*/
/*===========================================================================*/
// console_log_sink

namespace {
    const char *colour_for(log_level level) {
        switch (level) {
            case log_level::TRACE: {
                return "\x1b[90m";
            }
            case log_level::DEBUG: {
                return "\x1b[36m";
            }
            case log_level::INFO: {
                return "\x1b[0m";
            }
            case log_level::WARN: {
                return "\x1b[33m";
            }
            case log_level::ERR: {
                return "\x1b[31m";
            }
            case log_level::FATAL: {
                return "\x1b[1;41m";
            }
            case log_level::OFF: {
                return "\x1b[0m";
            }
        }
        return "\x1b[0m";
    }
}

std::FILE *conjure::core::detail::console_stream_for(log_level level) noexcept {
    return level >= log_level::WARN ? stderr : stdout;
}

void conjure::core::detail::write_console_line(std::FILE *stream, const log_message &message, bool useColour) {
    const std::string line = format_log_line(message);
    if (useColour) {
        std::fprintf(stream, "%s%s\x1b[0m\n", colour_for(message.level), line.c_str());
    } else {
        std::fprintf(stream, "%s\n", line.c_str());
    }

    if (message.level >= log_level::ERR) {
        std::fflush(stream);
    }
}

console_log_sink::console_log_sink(bool useColour) :
    log_sink(),
    mUseColour(useColour) {}

console_log_sink::~console_log_sink() {}

void console_log_sink::write(const log_message &message) {
    detail::write_console_line(detail::console_stream_for(message.level), message, mUseColour);
}

void console_log_sink::flush() {
    std::fflush(stdout);
    std::fflush(stderr);
}


/*===========================================================================*/
/*===========================================================================*/
// file_log_sink

struct file_log_sink::state {
    std::mutex mutex = {};
    std::ofstream stream = {};
};

file_log_sink::file_log_sink(const std::filesystem::path &path, bool truncate) :
    log_sink(),
    mState(std::make_unique<state>()) {
    std::error_code ignored = {};
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ignored);
    }

    const std::ios::openmode mode = std::ios::out | (truncate ? std::ios::trunc : std::ios::app);
    mState->stream.open(path, mode);
}

file_log_sink::~file_log_sink() {
    const std::scoped_lock lock(mState->mutex);
    if (mState->stream.is_open()) {
        mState->stream.flush();
    }
}

bool file_log_sink::is_open() const noexcept {
    return mState->stream.is_open();
}

void file_log_sink::write(const log_message &message) {
    const std::string line = format_log_line(message);

    const std::scoped_lock lock(mState->mutex);
    if (!mState->stream.is_open()) {
        return;
    }
    mState->stream << line << '\n';
    if (message.level >= log_level::ERR) {
        mState->stream.flush();
    }
}

void file_log_sink::flush() {
    const std::scoped_lock lock(mState->mutex);
    if (mState->stream.is_open()) {
        mState->stream.flush();
    }
}


/*===========================================================================*/
/*===========================================================================*/
// callback_log_sink

callback_log_sink::callback_log_sink(std::function<void(const log_message &)> callback) :
    log_sink(),
    mCallback(std::move(callback)) {}

callback_log_sink::~callback_log_sink() {}

void callback_log_sink::write(const log_message &message) {
    if (mCallback) {
        mCallback(message);
    }
}


/*===========================================================================*/
/*===========================================================================*/
// log namespace

void log::add_sink(std::shared_ptr<log_sink> sink) {
    if (!sink) {
        return;
    }
    logger_state &logger = state();
    const std::scoped_lock lock(logger.mutex);
    logger.sinks.push_back(std::move(sink));
}

void log::remove_sink(const std::shared_ptr<log_sink> &sink) {
    logger_state &logger = state();
    const std::scoped_lock lock(logger.mutex);
    std::erase(logger.sinks, sink);
}

void log::clear_sinks() {
    logger_state &logger = state();
    const std::scoped_lock lock(logger.mutex);
    logger.sinks.clear();
}

void log::set_console_enabled(bool enabled) {
    state().consoleEnabled.store(enabled, std::memory_order_relaxed);
}

bool log::console_enabled() noexcept {
    return state().consoleEnabled.load(std::memory_order_relaxed);
}

void log::set_global_level(log_level level) noexcept {
    state().globalLevel.store(level, std::memory_order_relaxed);
}

log_level log::global_level() noexcept {
    return state().globalLevel.load(std::memory_order_relaxed);
}

bool log::is_enabled(const log_category &category, log_level level) noexcept {
    if (level == log_level::OFF) {
        return false;
    }
    if (level < state().globalLevel.load(std::memory_order_relaxed)) {
        return false;
    }
    return category.is_enabled(level);
}

void log::write(const log_category &category, log_level level, const char *file, int line, std::string_view text) {
    log_message message = {};
    message.level = level;
    message.category = category.name();
    message.text = text;
    message.timestamp = std::chrono::system_clock::now();
    message.threadId = std::this_thread::get_id();
    message.file = file;
    message.line = line;

    const sink_snapshot sinks = snapshot_sinks(state());
    if (sinks.console) {
        sinks.console->write(message);
    }
    for (const std::shared_ptr<log_sink> &sink : sinks.sinks) {
        sink->write(message);
    }
}

void log::flush() {
    const sink_snapshot sinks = snapshot_sinks(state());
    if (sinks.console) {
        sinks.console->flush();
    }
    for (const std::shared_ptr<log_sink> &sink : sinks.sinks) {
        sink->flush();
    }
}

std::vector<log_category *> log::categories() {
    logger_state &logger = state();
    const std::scoped_lock lock(logger.mutex);
    return logger.categories;
}

log_category *log::find_category(std::string_view name) {
    logger_state &logger = state();
    const std::scoped_lock lock(logger.mutex);
    for (log_category *category : logger.categories) {
        if (name == category->name()) {
            return category;
        }
    }
    return nullptr;
}

bool log::set_category_level(std::string_view name, log_level level) {
    log_category *category = find_category(name);
    if (category == nullptr) {
        return false;
    }
    category->set_level(level);
    return true;
}
