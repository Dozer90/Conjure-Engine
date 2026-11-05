#include "conjure/core/assert.h"
#include "conjure/core/log.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>

using namespace conjure::core;

/*===========================================================================*/
/*===========================================================================*/
// Handler storage

namespace {
    std::atomic<assert_handler> sAssertHandler = &default_assert_handler;
}

assert_handler conjure::core::set_assert_handler(assert_handler handler) noexcept {
    if (handler == nullptr) {
        handler = &default_assert_handler;
    }
    return sAssertHandler.exchange(handler);
}

assert_handler conjure::core::current_assert_handler() noexcept {
    return sAssertHandler.load();
}


/*===========================================================================*/
/*===========================================================================*/
// Default handler

assert_action conjure::core::default_assert_handler(const assert_info &info) {
    // Go through the logger so the failure reaches files and the editor console, but also
    // write to stderr directly in case logging itself is what broke.
    const std::string line = info.message.empty()
      ? std::format("Assertion failed: {} ({}:{})", info.expression, info.file, info.line)
      : std::format("Assertion failed: {} - {} ({}:{})", info.expression, info.message, info.file, info.line);

    CONJURE_LOG_FATAL(LOG_CORE, "{}", line);
    log::flush();

    if (!log::console_enabled() || !log::is_enabled(LOG_CORE, log_level::FATAL)) {
        std::fputs(line.c_str(), stderr);
        std::fputc('\n', stderr);
        std::fflush(stderr);
    }

    return assert_action::BREAK;
}


/*===========================================================================*/
/*===========================================================================*/
// Reporting

bool conjure::core::detail::report_assert_failure(
  const char *expression,
  const char *file,
  int line,
  const char *function,
  std::string message
) {
    assert_info info = {};
    info.expression = expression;
    info.file = file;
    info.line = line;
    info.function = function;
    info.message = std::move(message);

    const assert_action action = sAssertHandler.load()(info);
    switch (action) {
        case assert_action::CONTINUE: {
            return false;
        }
        case assert_action::ABORT: {
            log::flush();
            std::abort();
        }
        case assert_action::BREAK: {
            return true;
        }
    }
    return true;
}
